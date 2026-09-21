#!/usr/bin/env python3
"""Multi-AI Orchestration: Codex (cố vấn/audit) ↔ AGY (thực thi)

Hệ thống điều phối tự động giữa 2 AI CLI cho dự án RnD_LoRa:
1. Codex (cố vấn) → viết plan.md
2. AGY (thực thi) → sửa code, viết work-log.md + self-check.md
3. Codex (audit)  → đối chiếu, ra verdict PASS/FAIL/PASS_WITH_NOTES

Usage:
    python .orchestrator/orchestrator.py tasks/<task-id>
"""

import json
import logging
import re
import subprocess
import sys
from datetime import datetime
from pathlib import Path
from typing import Optional

try:
    import tomllib  # Python 3.11+
except ImportError:
    try:
        import tomli as tomllib  # fallback cho Python 3.10
    except ImportError:
        tomllib = None

# UTF-8 encoding an toàn trên Windows
if sys.platform == "win32":
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if hasattr(sys.stderr, "reconfigure"):
        sys.stderr.reconfigure(encoding="utf-8", errors="replace")

# ─── Logging ──────────────────────────────────────────────────────
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    datefmt="%H:%M:%S",
)
log = logging.getLogger("orchestrator")

# ─── Constants & Defaults ─────────────────────────────────────────
DEFAULT_PROTECTED_BRANCHES = {"main"}
DEFAULT_BASE_BRANCH = "dev"
VALID_VERDICTS = {"PASS", "FAIL", "PASS_WITH_NOTES"}
DEFAULT_MAX_ROUNDS = 3
DEFAULT_CODEX_TIMEOUT = 600
DEFAULT_AGY_TIMEOUT = 1800


# ═══════════════════════════════════════════════════════════════════
#  1. CONFIG
# ═══════════════════════════════════════════════════════════════════

def load_config(orchestrator_dir: Path) -> dict:
    """Đọc config.toml, trả về dict với giá trị mặc định nếu thiếu."""
    config_path = orchestrator_dir / "config.toml"
    if config_path.exists() and tomllib:
        with open(config_path, "rb") as f:
            raw = tomllib.load(f)
    else:
        if not config_path.exists():
            log.warning("Không tìm thấy config.toml, dùng giá trị mặc định.")
        elif not tomllib:
            log.warning("Không có tomllib/tomli, dùng giá trị mặc định.")
        raw = {}

    git_cfg = raw.get("git", {})
    protected = set(git_cfg.get("protected_branches", list(DEFAULT_PROTECTED_BRANCHES)))
    base_branch = git_cfg.get("base_branch", DEFAULT_BASE_BRANCH)

    testing_cfg = raw.get("testing", {})
    test_commands = testing_cfg.get("commands", [])

    return {
        "max_rounds": raw.get("orchestrator", {}).get("max_rounds", DEFAULT_MAX_ROUNDS),
        "codex_timeout": raw.get("timeouts", {}).get("codex_seconds", DEFAULT_CODEX_TIMEOUT),
        "agy_timeout": raw.get("timeouts", {}).get("agy_seconds", DEFAULT_AGY_TIMEOUT),
        "sandbox_advisor": raw.get("sandbox", {}).get("codex_advisor", "read-only"),
        "sandbox_auditor": raw.get("sandbox", {}).get("codex_auditor", "read-only"),
        "sandbox_executor": raw.get("sandbox", {}).get("agy_executor", "workspace-write"),
        "early_escalate": raw.get("escalation", {}).get(
            "early_escalate_on_stagnation", True
        ),
        "base_branch": base_branch,
        "protected_branches": protected,
        "test_commands": test_commands,
    }


# ═══════════════════════════════════════════════════════════════════
#  2. GIT SAFETY (mục 10.1 trong đặc tả)
# ═══════════════════════════════════════════════════════════════════

def run_git(args: list[str], cwd: str | Path, check: bool = True) -> str:
    """Chạy lệnh git, trả về stdout. Raise RuntimeError nếu lỗi."""
    result = subprocess.run(
        ["git"] + args,
        cwd=str(cwd),
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    if check and result.returncode != 0:
        raise RuntimeError(f"git {' '.join(args)} lỗi: {result.stderr.strip()}")
    return result.stdout.strip()


def guard_not_protected(branch: str, protected_branches: set[str]) -> None:
    """Chặn mọi thao tác trên branch được bảo vệ."""
    if branch in protected_branches:
        raise RuntimeError(
            f"BỊ CHẶN: orchestrator không được phép thao tác trực tiếp "
            f"lên '{branch}'."
        )


def ensure_task_branch(task_id: str, project_root: Path, config: dict) -> str:
    """Tạo hoặc checkout nhánh task/<task_id> từ base_branch."""
    branch = f"task/{task_id}"
    base_branch = config["base_branch"]
    protected_branches = config["protected_branches"]

    guard_not_protected(branch, protected_branches)
    existing = run_git(["branch", "--list", branch], project_root, check=False)
    if not existing:
        log.info(f"Tạo nhánh mới: {branch} từ {base_branch}")
        run_git(["checkout", base_branch], project_root)
        run_git(["pull", "--ff-only"], project_root, check=False)
        run_git(["checkout", "-b", branch], project_root)
    else:
        run_git(["checkout", branch], project_root)
    return branch


def commit_checkpoint(
    task_id: str, round_num: int, project_root: Path,
    protected_branches: set[str], message_suffix: str = "",
) -> bool:
    """Commit checkpoint trên nhánh hiện tại (không phải protected)."""
    branch = run_git(["branch", "--show-current"], project_root)
    guard_not_protected(branch, protected_branches)
    run_git(["add", "-A"], project_root)
    result = subprocess.run(
        ["git", "commit", "-m",
         f"[{task_id}] round {round_num} {message_suffix}".strip()],
        cwd=str(project_root),
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    return result.returncode == 0


def reset_to_checkpoint(project_root: Path, checkpoint_sha: str, protected_branches: set[str]) -> None:
    """Reset hard về checkpoint cụ thể (không phải trên protected branch)."""
    branch = run_git(["branch", "--show-current"], project_root)
    guard_not_protected(branch, protected_branches)
    run_git(["reset", "--hard", checkpoint_sha], project_root)


def push_branch(branch: str, project_root: Path, protected_branches: set[str]) -> None:
    """Chỉ push nhánh task/*, không bao giờ push main."""
    guard_not_protected(branch, protected_branches)
    if not branch.startswith("task/"):
        raise RuntimeError(
            f"BỊ CHẶN: chỉ được push nhánh task/*, không phải '{branch}'"
        )
    run_git(["push", "-u", "origin", branch], project_root)


def get_changed_files(
    project_root: Path, base_ref: str = "HEAD~1",
) -> list[str]:
    """Lấy danh sách file đã thay đổi so với base_ref."""
    output = run_git(
        ["diff", "--name-only", base_ref], project_root, check=False,
    )
    return [f for f in output.splitlines() if f]


# ═══════════════════════════════════════════════════════════════════
#  3. FILE I/O & FRONTMATTER
# ═══════════════════════════════════════════════════════════════════

def read_file(path: Path) -> str:
    """Đọc file UTF-8 (tự xử lý BOM nếu có), trả về chuỗi rỗng nếu không tồn tại."""
    if not path.exists():
        return ""
    return path.read_text(encoding="utf-8-sig")


def write_file(path: Path, content: str) -> None:
    """Ghi file UTF-8, tạo thư mục cha nếu cần."""
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def clean_output_content(text: str) -> str:
    """Loại bỏ code fence bọc ngoài (```markdown ... ```) nếu có."""
    text = text.strip()
    if text.startswith("```"):
        text = re.sub(r"^```[a-zA-Z0-9_-]*\r?\n", "", text)
        text = re.sub(r"\r?\n```\s*$", "", text.strip())
    return text.strip()


def parse_frontmatter(text: str) -> dict:
    """Trích xuất YAML frontmatter (hỗ trợ cả LF và CRLF, unwrap code block)."""
    text = clean_output_content(text)
    match = re.search(r"^---\r?\n(.+?)\r?\n---", text, re.DOTALL | re.MULTILINE)
    if not match:
        return {}
    fm = {}
    for line in match.group(1).splitlines():
        if ":" in line:
            key, _, value = line.partition(":")
            fm[key.strip()] = value.strip()
    return fm


def validate_output_file(
    path: Path, required_fields: list[str] | None = None,
) -> bool:
    """Kiểm tra file tồn tại, non-empty, có frontmatter hợp lệ (mục 10.6)."""
    if not path.exists():
        log.error(f"File không tồn tại: {path}")
        return False
    content = read_file(path)
    if not content.strip():
        log.error(f"File rỗng: {path}")
        return False
    cleaned = clean_output_content(content)
    if cleaned != content.strip():
        write_file(path, cleaned)
        content = cleaned
    if required_fields:
        fm = parse_frontmatter(content)
        for field in required_fields:
            if field not in fm:
                log.error(f"File {path} thiếu frontmatter field: {field}")
                return False
    return True


def extract_verdict(audit_path: Path) -> str:
    """Trích xuất verdict từ audit-report.md.

    Raise RuntimeError nếu không tìm thấy verdict hợp lệ (mục 11).
    Không mặc định fallback — bắt buộc phải có verdict rõ ràng.
    """
    content = read_file(audit_path)
    fm = parse_frontmatter(content)
    verdict = fm.get("verdict", "").strip().upper()
    verdict = verdict.split("#")[0].strip()

    if verdict not in VALID_VERDICTS:
        raise RuntimeError(
            f"Verdict không hợp lệ trong {audit_path}: '{verdict}'. "
            f"Phải là một trong: {VALID_VERDICTS}"
        )
    return verdict


def count_fail_items(audit_path: Path) -> int:
    """Đếm số mục FAIL trong bảng 'Đối chiếu tiêu chí' của audit-report.

    Dùng cho circuit breaker / stagnation check (mục 11).
    """
    content = read_file(audit_path)
    count = 0
    for line in content.splitlines():
        if re.search(r"\|\s*FAIL\s*\|", line, re.IGNORECASE):
            count += 1
    return count


# ═══════════════════════════════════════════════════════════════════
#  4. STATUS MANAGEMENT
# ═══════════════════════════════════════════════════════════════════

def reconcile_status_with_disk(status: dict, task_dir: Path) -> dict:
    """Đối chiếu status.json với file thực tế trên đĩa (mục 10.6)."""
    phase = status.get("phase", "planning")

    if phase == "planning" or phase == "error":
        plan_ok = validate_output_file(task_dir / "plan.md", ["role", "task_id"])
        if plan_ok:
            log.info("Resume: plan.md đã tồn tại và hợp lệ, chuyển sang executing.")
            status["phase"] = "executing"
            phase = "executing"

    if phase == "executing":
        wl_ok = validate_output_file(
            task_dir / "work-log.md", ["role", "task_id"],
        )
        sc_ok = validate_output_file(task_dir / "self-check.md", ["role"])
        if wl_ok and sc_ok:
            log.info(
                "Resume: work-log.md + self-check.md đã tồn tại, "
                "chuyển sang auditing."
            )
            status["phase"] = "auditing"

    elif phase == "auditing":
        ar_ok = validate_output_file(
            task_dir / "audit-report.md", ["role", "verdict"],
        )
        if ar_ok:
            try:
                verdict = extract_verdict(task_dir / "audit-report.md")
                log.info(
                    f"Resume: audit-report.md đã tồn tại, verdict = {verdict}"
                )
                status["verdict"] = verdict
                if verdict in ("PASS", "PASS_WITH_NOTES"):
                    status["phase"] = "done"
                else:
                    status["phase"] = "executing"
                    status["round"] = status.get("round", 1) + 1
            except RuntimeError:
                pass

    return status


def load_status(task_dir: Path, task_id: str) -> dict:
    """Đọc status.json, tạo mới nếu chưa có."""
    status_path = task_dir / "status.json"
    if status_path.exists():
        with open(status_path, encoding="utf-8-sig") as f:
            status = json.load(f)
        status = reconcile_status_with_disk(status, task_dir)
        return status

    return {
        "task_id": task_id,
        "round": 1,
        "phase": "planning",
        "verdict": None,
        "history": [],
        "fail_counts": [],
    }


def save_status(task_dir: Path, status: dict) -> None:
    """Ghi status.json."""
    status_path = task_dir / "status.json"
    with open(status_path, "w", encoding="utf-8") as f:
        json.dump(status, f, indent=2, ensure_ascii=False)


# ═══════════════════════════════════════════════════════════════════
#  5. LOCK FILE (mục 10.5)
# ═══════════════════════════════════════════════════════════════════

def is_pid_alive(pid: int) -> bool:
    """Kiểm tra process có đang chạy không (hỗ trợ Windows và POSIX)."""
    if sys.platform == "win32":
        import ctypes
        PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
        handle = ctypes.windll.kernel32.OpenProcess(
            PROCESS_QUERY_LIMITED_INFORMATION, False, pid
        )
        if not handle:
            return False
        exit_code = ctypes.c_ulong()
        ctypes.windll.kernel32.GetExitCodeProcess(handle, ctypes.byref(exit_code))
        ctypes.windll.kernel32.CloseHandle(handle)
        return exit_code.value == 259  # STILL_ACTIVE
    else:
        import os
        try:
            os.kill(pid, 0)
            return True
        except OSError:
            return False


def acquire_lock(orchestrator_dir: Path) -> Path:
    """Tạo lock file để tránh chạy song song."""
    lock_path = orchestrator_dir / ".lock"
    if lock_path.exists():
        try:
            pid = int(lock_path.read_text().strip())
            if is_pid_alive(pid):
                raise RuntimeError(
                    f"Orchestrator đang chạy (PID {pid}). "
                    f"Nếu đây là lỗi, hãy xóa file {lock_path}"
                )
            else:
                log.warning(f"Lock file cũ của PID {pid} đã chết, tự động dọn dẹp.")
                lock_path.unlink()
        except RuntimeError:
            raise
        except Exception:
            log.warning("Lock file cũ không hợp lệ, xóa và tiếp tục.")
            lock_path.unlink()

    import os
    lock_path.write_text(str(os.getpid()))
    return lock_path


def release_lock(lock_path: Path) -> None:
    """Xóa lock file."""
    if lock_path.exists():
        lock_path.unlink()


# ═══════════════════════════════════════════════════════════════════
#  6. CLI CALLERS (mục 7)
# ═══════════════════════════════════════════════════════════════════

def save_log(
    task_dir: Path, round_num: int, role: str,
    stdout: str, stderr: str, exit_code: int,
) -> None:
    """Lưu raw stdout/stderr mỗi lần gọi CLI (mục 10.7)."""
    log_dir = task_dir / "logs"
    log_dir.mkdir(exist_ok=True)
    log_path = log_dir / f"round-{round_num}-{role}.log"
    content = (
        f"=== {role} | round {round_num} | "
        f"{datetime.now().isoformat()} ===\n"
        f"Exit code: {exit_code}\n"
        f"\n--- STDOUT ---\n{stdout}\n"
        f"\n--- STDERR ---\n{stderr}\n"
    )
    with open(log_path, "a", encoding="utf-8") as f:
        f.write(content)


def run_codex_advisor(
    task_dir: Path, config: dict, round_num: int,
) -> None:
    """Gọi Codex CLI vai cố vấn → ghi plan.md (mục 7, 8, 11)."""
    orch_dir = task_dir.parent.parent / ".orchestrator"

    prompt_template = read_file(orch_dir / "prompts" / "advisor.txt")
    task_content = read_file(task_dir / "task.md")
    knowledge = read_file(orch_dir / "knowledge.md")
    lessons = read_file(orch_dir / "lessons-learned.md")

    prompt = (
        f"{prompt_template}\n\n"
        f"--- TASK.MD ---\n{task_content}\n\n"
        f"--- KNOWLEDGE.MD ---\n{knowledge}\n\n"
        f"--- LESSONS-LEARNED.MD ---\n{lessons}"
    )

    output_file = task_dir / "plan.md"
    log.info(
        f"[Round {round_num}] Gọi Codex advisor "
        f"(sandbox={config['sandbox_advisor']})..."
    )

    cmd = [
        "codex", "exec",
        "--sandbox", config["sandbox_advisor"],
        "--output-last-message", str(output_file),
        "--json",
        "-",
    ]

    result = subprocess.run(
        cmd,
        input=prompt,
        cwd=str(task_dir),
        capture_output=True,
        text=True,
        encoding="utf-8",
        timeout=config["codex_timeout"],
    )

    save_log(
        task_dir, round_num, "advisor",
        result.stdout, result.stderr, result.returncode,
    )

    if not validate_output_file(output_file, ["role", "task_id"]):
        raise RuntimeError(
            f"Codex advisor không tạo được plan.md hợp lệ "
            f"(round {round_num})"
        )

    log.info(f"[Round {round_num}] plan.md đã được tạo thành công.")


def run_agy_executor(
    task_dir: Path, config: dict, round_num: int,
) -> None:
    """Gọi AGY CLI vai thực thi → AGY tự ghi work-log.md + self-check.md (mục 7, 8, 11)."""
    orch_dir = task_dir.parent.parent / ".orchestrator"

    prompt_template = read_file(orch_dir / "prompts" / "executor.txt")
    task_content = read_file(task_dir / "task.md")
    plan_content = read_file(task_dir / "plan.md")
    knowledge = read_file(orch_dir / "knowledge.md")

    prompt = (
        f"{prompt_template}\n\n"
        f"--- TASK.MD ---\n{task_content}\n\n"
        f"--- PLAN.MD ---\n{plan_content}\n\n"
        f"--- KNOWLEDGE.MD ---\n{knowledge}"
    )

    if round_num > 1:
        audit_content = read_file(task_dir / "audit-report.md")
        prompt += (
            f"\n\n--- AUDIT-REPORT.MD (round trước) ---\n"
            f"{audit_content}"
        )
        prompt += (
            "\n\nCHỈ SỬA CÁC MỤC FAIL trong audit report trên. "
            "Không thay đổi các mục đã PASS."
        )

    prompt += (
        f"\n\n## CHỈ DẪN THỰC THI BẮT BUỘC CHO PHIÊN NÀY:\n"
        f"1. BẮT ĐẦU NGAY BẰNG CÁCH TẠO MÃ NGUỒN: Sử dụng tool tạo các file code và file test theo plan.md.\n"
        f"2. Chạy kiểm thử xác minh sau khi viết code.\n"
        f"3. BẮT BUỘC ghi toàn bộ nhật ký thực thi vào file: {task_dir / 'work-log.md'}\n"
        f"4. BẮT BUỘC ghi bảng tự kiểm tra vào file: {task_dir / 'self-check.md'}\n"
        f"5. task_id trong frontmatter là: {task_dir.name}, round: {round_num}.\n"
        f"TUYỆT ĐỐI KHÔNG DỪNG PHIÊN KHI CHƯA TẠO XONG 2 FILE work-log.md VÀ self-check.md!"
    )

    log.info(f"[Round {round_num}] Gọi AGY executor...")

    if len(prompt) > 24000:
        prompt_file = task_dir / "logs" / f"round-{round_num}-agy-prompt.txt"
        write_file(prompt_file, prompt)
        agy_prompt = (
            f"BẮT BUỘC: Sử dụng tool xem file để đọc toàn bộ nội dung hướng dẫn, nhiệm vụ và kế hoạch thực thi trong file: "
            f"{prompt_file.resolve()} và tiến hành thực hiện đúng theo các yêu cầu đó."
        )
    else:
        agy_prompt = prompt

    cmd = [
        "agy",
        "--dangerously-skip-permissions",
        "--output-format", "text",
        "--print", agy_prompt,
    ]

    result = subprocess.run(
        cmd,
        cwd=str(task_dir.parent.parent),  # project root
        capture_output=True,
        text=True,
        encoding="utf-8",
        timeout=config["agy_timeout"],
    )

    save_log(
        task_dir, round_num, "executor",
        result.stdout, result.stderr, result.returncode,
    )

    wl_valid = validate_output_file(task_dir / "work-log.md", ["role"])
    sc_valid = validate_output_file(task_dir / "self-check.md", ["role"])

    if not wl_valid or not sc_valid:
        raise RuntimeError(
            f"AGY executor không tạo được output hợp lệ "
            f"(round {round_num}). "
            f"work-log.md: {'OK' if wl_valid else 'THIẾU/LỖI'}, "
            f"self-check.md: {'OK' if sc_valid else 'THIẾU/LỖI'}"
        )

    log.info(
        f"[Round {round_num}] AGY executor hoàn tất, "
        f"output đã được validate."
    )


def run_codex_auditor(
    task_dir: Path, config: dict, round_num: int,
    test_result: str = "",
) -> None:
    """Gọi Codex CLI vai audit → ghi audit-report.md (mục 7, 8, 11)."""
    orch_dir = task_dir.parent.parent / ".orchestrator"

    prompt_template = read_file(orch_dir / "prompts" / "auditor.txt")
    task_content = read_file(task_dir / "task.md")
    plan_content = read_file(task_dir / "plan.md")
    worklog_content = read_file(task_dir / "work-log.md")
    selfcheck_content = read_file(task_dir / "self-check.md")

    prompt = (
        f"{prompt_template}\n\n"
        f"--- TASK.MD ---\n{task_content}\n\n"
        f"--- PLAN.MD ---\n{plan_content}\n\n"
        f"--- WORK-LOG.MD ---\n{worklog_content}\n\n"
        f"--- SELF-CHECK.MD ---\n{selfcheck_content}"
    )

    if test_result:
        prompt += (
            f"\n\n--- KẾT QUẢ TEST/BUILD THẬT "
            f"(do orchestrator chạy khách quan) ---\n{test_result}"
        )
        prompt += (
            "\n\nĐÂY LÀ BẰNG CHỨNG KHÁCH QUAN — verdict phải dựa trên "
            "kết quả này, không chỉ tin self-check."
        )

    output_file = task_dir / "audit-report.md"
    log.info(
        f"[Round {round_num}] Gọi Codex auditor "
        f"(sandbox={config['sandbox_auditor']})..."
    )

    cmd = [
        "codex", "exec",
        "--sandbox", config["sandbox_auditor"],
        "--output-last-message", str(output_file),
        "--json",
        "-",
    ]

    result = subprocess.run(
        cmd,
        input=prompt,
        cwd=str(task_dir),
        capture_output=True,
        text=True,
        encoding="utf-8",
        timeout=config["codex_timeout"],
    )

    save_log(
        task_dir, round_num, "auditor",
        result.stdout, result.stderr, result.returncode,
    )

    if not validate_output_file(output_file, ["role", "verdict"]):
        raise RuntimeError(
            f"Codex auditor không tạo được audit-report.md hợp lệ "
            f"(round {round_num})"
        )

    log.info(
        f"[Round {round_num}] audit-report.md đã được tạo thành công."
    )


# ═══════════════════════════════════════════════════════════════════
#  7. OBJECTIVE VERIFICATION (mục 10.2)
# ═══════════════════════════════════════════════════════════════════

def run_objective_tests(project_root: Path, config: dict) -> str:
    """Chạy test/build thật, trả về output để đưa vào prompt audit.

    Orchestrator tự chạy độc lập — không tin self-report của AI.
    """
    results = []
    test_commands = config.get("test_commands", [])

    if test_commands:
        for cmd in test_commands:
            cmd_str = " ".join(cmd) if isinstance(cmd, list) else str(cmd)
            try:
                r = subprocess.run(
                    cmd_str,
                    shell=True,
                    cwd=str(project_root),
                    capture_output=True,
                    text=True,
                    encoding="utf-8",
                    timeout=300,
                )
                results.append(
                    f"=== Lệnh: {cmd_str} ===\n"
                    f"Exit code: {r.returncode}\n{r.stdout}\n{r.stderr}"
                )
            except Exception as e:
                results.append(f"=== Lệnh: {cmd_str} ===\nLỗi: {e}")
    else:
        tests_dir = project_root / "tests"
        if tests_dir.exists():
            try:
                r = subprocess.run(
                    [sys.executable, "-m", "pytest", "tests/", "-v", "--tb=short"],
                    cwd=str(project_root),
                    capture_output=True,
                    text=True,
                    encoding="utf-8",
                    timeout=120,
                )
                results.append(
                    f"=== pytest ===\n"
                    f"Exit code: {r.returncode}\n{r.stdout}\n{r.stderr}"
                )
            except Exception as e:
                results.append(f"=== pytest ===\nLỗi: {e}")
        else:
            results.append("Chưa có thư mục tests/ để chạy tự động.")

    return "\n\n".join(results)


# ═══════════════════════════════════════════════════════════════════
#  8. SCOPE GUARD (mục 10.4)
# ═══════════════════════════════════════════════════════════════════

def check_scope_violation(
    task_dir: Path, project_root: Path,
) -> Optional[str]:
    """Kiểm tra AGY có sửa file bị cấm không.

    Đọc ràng buộc từ task.md, đối chiếu với git diff.
    """
    task_content = read_file(task_dir / "task.md")

    forbidden_patterns = []
    in_constraints = False
    for line in task_content.splitlines():
        if line.strip().startswith("## Ràng buộc"):
            in_constraints = True
            continue
        if in_constraints and line.strip().startswith("## "):
            break
        if in_constraints and "không được sửa" in line.lower():
            paths = re.findall(r"`([^`]+)`", line)
            forbidden_patterns.extend(paths)
        if in_constraints and ".orchestrator" in line.lower():
            forbidden_patterns.append(".orchestrator/")

    if not forbidden_patterns:
        return None

    changed = get_changed_files(project_root)

    violations = []
    for f in changed:
        for pattern in forbidden_patterns:
            if f.startswith(pattern) or pattern in f:
                violations.append(f"  - {f} (vi phạm: {pattern})")

    if violations:
        return (
            "SCOPE VIOLATION — AGY đã sửa file bị cấm:\n"
            + "\n".join(violations)
        )
    return None


# ═══════════════════════════════════════════════════════════════════
#  9. VERDICT RETRY (mục 10.3)
# ═══════════════════════════════════════════════════════════════════

def extract_verdict_with_retry(
    task_dir: Path, config: dict, round_num: int,
    test_result: str, max_retries: int = 2,
) -> str:
    """Trích verdict, gọi lại Codex nếu định dạng sai (tối đa max_retries)."""
    for attempt in range(max_retries + 1):
        try:
            return extract_verdict(task_dir / "audit-report.md")
        except RuntimeError as e:
            if attempt < max_retries:
                log.warning(
                    f"Verdict không hợp lệ "
                    f"(lần {attempt + 1}/{max_retries + 1}), "
                    f"gọi lại Codex auditor..."
                )
                run_codex_auditor(
                    task_dir, config, round_num, test_result,
                )
            else:
                raise RuntimeError(
                    f"Sau {max_retries + 1} lần thử, vẫn không trích "
                    f"được verdict hợp lệ. Lỗi gốc: {e}"
                )
    raise RuntimeError("Unexpected: verdict retry loop exited without result")


# ═══════════════════════════════════════════════════════════════════
#  10. LESSONS LEARNED (mục 7, 8 — bước riêng biệt)
# ═══════════════════════════════════════════════════════════════════

def maybe_update_lessons_learned(
    task_dir: Path, config: dict, round_num: int,
) -> None:
    """Nếu round > 1 (có FAIL đã sửa), gọi Codex để append lessons-learned."""
    if round_num <= 1:
        return

    orch_dir = task_dir.parent.parent / ".orchestrator"
    lessons_path = orch_dir / "lessons-learned.md"

    audit_content = read_file(task_dir / "audit-report.md")
    worklog_content = read_file(task_dir / "work-log.md")

    prompt = (
        f"Đọc audit-report.md và work-log.md bên dưới.\n"
        f"Nếu có lỗi ĐÁNG NHỚ (không phải lỗi vụn vặt như gõ sai biến), "
        f"hãy append 1 mục ngắn gọn vào cuối file lessons-learned.md "
        f"tại: {lessons_path}\n"
        f"Nếu không có gì đáng ghi, KHÔNG sửa gì cả.\n\n"
        f"--- AUDIT-REPORT.MD ---\n{audit_content}\n\n"
        f"--- WORK-LOG.MD ---\n{worklog_content}\n\n"
        f"--- LESSONS-LEARNED.MD HIỆN TẠI ---\n"
        f"{read_file(lessons_path)}"
    )

    log.info("Gọi Codex để cập nhật lessons-learned.md (nếu cần)...")

    cmd = [
        "codex", "exec",
        "--sandbox", "workspace-write",
        "--json",
        "-",
    ]

    try:
        result = subprocess.run(
            cmd,
            input=prompt,
            cwd=str(orch_dir.parent),
            capture_output=True,
            text=True,
            encoding="utf-8",
            timeout=300,
        )
        save_log(
            task_dir, round_num, "lessons",
            result.stdout, result.stderr, result.returncode,
        )
    except subprocess.TimeoutExpired:
        log.warning("Timeout khi cập nhật lessons-learned.md — bỏ qua.")


# ═══════════════════════════════════════════════════════════════════
#  11. MAIN ORCHESTRATION LOOP
# ═══════════════════════════════════════════════════════════════════

def orchestrate(task_dir_str: str) -> None:
    """Vòng lặp điều phối chính (mục 8)."""
    task_dir = Path(task_dir_str).resolve()
    if not task_dir.exists():
        log.error(f"Thư mục task không tồn tại: {task_dir}")
        sys.exit(1)

    task_md = task_dir / "task.md"
    if not task_md.exists():
        log.error(f"Không tìm thấy task.md trong {task_dir}")
        sys.exit(1)

    project_root = task_dir.parent.parent
    orch_dir = project_root / ".orchestrator"

    config = load_config(orch_dir)
    max_rounds = config["max_rounds"]
    protected_branches = config["protected_branches"]
    base_branch = config["base_branch"]

    fm = parse_frontmatter(read_file(task_md))
    task_id = fm.get("task_id", task_dir.name)

    log.info(f"{'═' * 60}")
    log.info(f"  Bắt đầu orchestrate task: {task_id}")
    log.info(f"  Base branch: {base_branch}")
    log.info(f"  Max rounds: {max_rounds}")
    log.info(f"{'═' * 60}")

    lock_path = acquire_lock(orch_dir)

    try:
        status = None
        branch = None
        try:
            branch = ensure_task_branch(task_id, project_root, config)
            log.info(f"Đang trên nhánh: {branch}")
        except RuntimeError as e:
            log.warning(
                f"Git branch error (có thể không phải git repo): {e}"
            )

        status = load_status(task_dir, task_id)
        round_num = status["round"]
        phase = status["phase"]

        if phase == "done":
            log.info("Task đã hoàn thành từ trước.")
            return
        if phase == "escalated":
            log.info("Task đã bị escalate, cần xử lý thủ công.")
            return

        while round_num <= max_rounds:
            log.info(f"\n{'─' * 60}")
            log.info(f"Round {round_num}/{max_rounds} — Phase: {phase}")
            log.info(f"{'─' * 60}")

            # ── PHASE 1: PLANNING ──
            if phase == "planning":
                run_codex_advisor(task_dir, config, round_num)
                status["history"].append(f"codex:plan:r{round_num}")
                phase = "executing"
                status["phase"] = phase
                save_status(task_dir, status)

            # ── PHASE 2: EXECUTING ──
            if phase == "executing":
                run_agy_executor(task_dir, config, round_num)
                status["history"].append(f"agy:execute:r{round_num}")

                if branch:
                    committed = commit_checkpoint(
                        task_id, round_num, project_root, protected_branches, "after-agy",
                    )
                    if committed:
                        log.info(
                            f"Git checkpoint committed "
                            f"(round {round_num})."
                        )

                violation = check_scope_violation(task_dir, project_root)
                if violation:
                    log.error(f"SCOPE VIOLATION: {violation}")
                    log.error("Tự động đánh dấu FAIL — không cần đợi audit.")
                    auto_fail_report = (
                        f"---\nrole: codex-auditor\n"
                        f"task_id: {task_id}\n"
                        f"round: {round_num}\nverdict: FAIL\n---\n\n"
                        f"# Báo cáo audit (auto-generated)\n\n"
                        f"## Lý do FAIL\n{violation}\n"
                    )
                    write_file(
                        task_dir / "audit-report.md", auto_fail_report,
                    )
                    status["fail_counts"].append(1)
                    round_num += 1
                    status["round"] = round_num
                    phase = "executing"
                    status["phase"] = phase
                    save_status(task_dir, status)
                    continue

                phase = "auditing"
                status["phase"] = phase
                save_status(task_dir, status)

            # ── PHASE 3: AUDITING ──
            if phase == "auditing":
                test_result = run_objective_tests(project_root, config)
                log.info(
                    f"Kết quả test khách quan:\n"
                    f"{test_result[:500]}..."
                )

                run_codex_auditor(
                    task_dir, config, round_num, test_result,
                )
                status["history"].append(f"codex:audit:r{round_num}")

                verdict = extract_verdict_with_retry(
                    task_dir, config, round_num, test_result,
                )
                status["verdict"] = verdict
                log.info(f"Verdict round {round_num}: {verdict}")

                if verdict in ("PASS", "PASS_WITH_NOTES"):
                    phase = "done"
                    status["phase"] = phase
                    save_status(task_dir, status)
                    break
                else:
                    fail_count = count_fail_items(
                        task_dir / "audit-report.md",
                    )
                    status["fail_counts"].append(fail_count)

                    if (
                        len(status["fail_counts"]) >= 2
                        and config["early_escalate"]
                        and status["fail_counts"][-1]
                        >= status["fail_counts"][-2]
                    ):
                        log.error(
                            f"Stagnation detected: FAIL count không giảm "
                            f"({status['fail_counts'][-2]} → "
                            f"{status['fail_counts'][-1]}). "
                            f"Escalate sớm."
                        )
                        phase = "escalated"
                        status["phase"] = phase
                        save_status(task_dir, status)
                        break

                    round_num += 1
                    status["round"] = round_num
                    phase = "executing"
                    status["phase"] = phase
                    save_status(task_dir, status)

                    if round_num > max_rounds:
                        break

        # ── POST-LOOP ──
        if phase == "done":
            log.info(f"\n{'═' * 60}")
            log.info(
                f"  ✅ Task {task_id} HOÀN THÀNH "
                f"sau {round_num} round(s)."
            )
            log.info(f"{'═' * 60}")

            if branch:
                commit_checkpoint(
                    task_id, round_num, project_root, protected_branches, "DONE",
                )
                log.info(
                    f"\nXem diff: "
                    f"git log {base_branch}..{branch}"
                )
                log.info(
                    f"Merge thủ công: "
                    f"git checkout {base_branch} && "
                    f"git merge {branch}"
                )

            maybe_update_lessons_learned(task_dir, config, round_num)

        elif phase != "escalated":
            phase = "escalated"
            status["phase"] = phase
            save_status(task_dir, status)

        if phase == "escalated":
            log.warning(f"\n{'═' * 60}")
            log.warning(
                f"  ⚠️ Task {task_id} bị ESCALATE "
                f"sau {round_num - 1} round(s). "
                f"Cần xử lý thủ công."
            )
            log.warning(f"{'═' * 60}")
            log.warning(
                f"Xem chi tiết: {task_dir / 'audit-report.md'}"
            )
            if branch:
                log.warning(
                    f"Nhánh: {branch} (không merge tự động)"
                )

    except Exception as e:
        log.error(f"Lỗi nghiêm trọng: {e}", exc_info=True)
        if status is not None:
            status["phase"] = "error"
            save_status(task_dir, status)
        raise
    finally:
        release_lock(lock_path)


# ═══════════════════════════════════════════════════════════════════
#  ENTRY POINT
# ═══════════════════════════════════════════════════════════════════

def main():
    if len(sys.argv) < 2:
        print(
            "Usage: python .orchestrator/orchestrator.py tasks/<task-id>\n"
            "\n"
            "Ví dụ: python .orchestrator/orchestrator.py "
            "tasks/T-2026-0920-01"
        )
        sys.exit(1)

    task_dir = sys.argv[1]
    orchestrate(task_dir)


if __name__ == "__main__":
    main()
