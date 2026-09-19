# Multi-AI Orchestration (Codex ↔ AGY) - RnD_LoRa

Hệ thống điều phối tự động giữa Codex (cố vấn/audit) và Antigravity CLI - AGY (thực thi/tự kiểm) theo đặc tả `multi-ai-orchestration-spec`.

## Cấu trúc thư mục

```text
.
├── .orchestrator/
│   ├── orchestrator.py              ← script điều phối chính
│   ├── config.toml                  ← cấu hình max_rounds, timeout, sandbox
│   ├── knowledge.md                 ← kiến trúc & quy ước dự án (người viết, AI chỉ đọc)
│   ├── lessons-learned.md           ← lỗi đã gặp & cách tránh (AI được phép tự thêm)
│   ├── hooks/
│   │   └── pre-push                 ← git hook chặn push vào main
│   └── prompts/
│       ├── advisor.txt              ← template prompt cho Codex (vai cố vấn)
│       ├── auditor.txt              ← template prompt cho Codex (vai audit)
│       └── executor.txt             ← template prompt cho AGY
└── tasks/
    └── <task-id>/
        ├── task.md                  ← input gốc, bất biến sau khi tạo
        ├── plan.md                  ← Codex viết (giai đoạn cố vấn)
        ├── work-log.md              ← AGY viết (giai đoạn thực thi)
        ├── self-check.md            ← AGY tự kiểm trước khi nộp
        ├── audit-report.md          ← Codex viết (giai đoạn audit)
        └── status.json              ← trạng thái máy đọc được
```

## Cách chạy task

1. Tạo thư mục `tasks/<task-id>` (tham khảo mẫu tại `tasks/template/task.md`).
2. Viết yêu cầu vào file `tasks/<task-id>/task.md`.
3. Chạy lệnh:
   ```bash
   python .orchestrator/orchestrator.py tasks/<task-id>
   ```
