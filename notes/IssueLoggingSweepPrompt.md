# Issue Logging Sweep (prompt)

Run with effort **high**. Paste into a new session, or say "run the issue logging
sweep".

---

# Task: widen Winnow's issue logging, and finish the issue-report pipeline

## What already exists

Built 2026-11-23. Check `git log` / `git status` to see whether it is committed.

Help > Report an Issue... (`MW::reportIssue`, Main/diagnostics.cpp) offers two routes:
- **Report on GitHub**: opens the towhee/Winnow64 `bug_report.yml` issue form with the
  `version` and `system` fields pre-filled from the URL, and reveals WinnowReport.txt
  so the user can drag it in. GitHub needs an account and is PUBLIC.
- **Send by Email**: `MW::mailReport(hideFolders)` attaches WinnowReport.txt
  automatically. On macOS it uses AppleScript, but only for the default mail app
  (`Mac::defaultMailAppId`): Apple Mail, or Microsoft Outlook (New Outlook 16.115
  verified). Windows uses Simple MAPI. Linux and any other mail app get a pre-filled
  mailto: plus the file revealed in the file browser.
- A "Hide folder names in the report" checkbox, ticked by default.
- Issue forms are in `.github/ISSUE_TEMPLATE/`. Developer docs are in
  notes/Documentation.md 22.7, 22.7.1 and 22.7.2. The user help page is
  Docs/winnowhelp.html, "Reporting a problem".

`MW::prepareReport(hideFolders)` writes `<AppData>/Log/Report/WinnowReport.txt`:
- a header from `issueSystemInfo()`
- issues from the last 3 sessions of WinnowIssueLog.txt, each repeat collapsed to
  "(repeated N times)", keyed on Issue::toString's fixed columns, chars 20 to 129;
  capped at 600 lines
- the last 200 non-blank lines of WinnowLog.txt
- the newest Winnow crash report from the last 7 days, first 256 KB (macOS
  DiagnosticReports, Windows WER Report.wer in UTF-16; Linux has none)
- the home folder replaced with "~"; hideFolders turns paths into `~/…/file.ext`

Why it is built this way:
- The first email attempts were 36 MB of raw logs. They were rejected and sat in
  Mail's Outbox, queued to go out from someone else's account.
- On a real profile, 35 MB of logs became a 28 KB report.
- Limits to respect: Outlook.com / most mail servers about 20-25 MB per message;
  the GitHub URL about 8 KB, so nothing large goes in the query; GitHub attachments
  25 MB per file. The target is a report of tens of KB, well under 1 MB in the worst
  case.

Fixes already made to log noise at the source:
- Utilities::trimLog(14) trims WinnowLog.txt at startup.
- Empty G::log("") calls no longer reach the file.
- MW::developPixelSource now logs under G::isLogger.
- The CR3 "Box X is unknown" warning now uses issueDedup.
- IssueLog::run drains the whole queue on each wake and again on stop; it used to
  write one line every 2 s and lost the rest at quit.
- The session marker now carries the version.

First catch from the new report: Catalog NOT NULL failures caused by NaN (Sony
FocusLocation 0/0). Fixed with Catalog `real()` and the
tst_catalog::nonFiniteNumbersStillCommit test.

## Goal 1: find what should be logged but isn't

Sweep the whole codebase for failures that are swallowed silently or reported only
by qDebug/qWarning, so they reach the issue log (G::issue / G::issueDedup in
Main/global.cpp, Log/issue.*). Use Explore subagents in parallel, one per area:
- File I/O, sidecars, the catalog/SQLite layers (Cache/, Datamodel/userdb)
- Image decoders and metadata parsers (ImageFormats/, Metadata/)
- Develop pipeline, masks, inference/ORT model loading (Develop/, Utilities/inference)
- Caches, threads, ingest, export, update/network (Cache/, Main/, Utilities/)

Look for:
- ignored return values: QFile::open, QSqlQuery::exec / commit, QImage::load /
  save, rename / remove, decode functions returning false
- `catch (...)` blocks with no report
- qWarning/qDebug used as the only error report on a real failure path
- non-finite math headed for storage or display
- network errors, model download / load failures, and SQL errors
  (always include lastError().text())
- thread or worker timeouts and aborted operations

Before making mass edits, produce a ranked list: file:line, what fails, the proposed
severity, and the call (issue vs issueDedup). I will approve it.

## Goal 2: keep performance cost near zero

- Log on failure paths only. Never in per-pixel, per-row or per-paint loops on
  success.
- Per-file or per-row repeats use G::issueDedup.
- Do not build message strings before the severity check where that path is hot.
  If it is, gate it with a cheap check.
- Leave IssueLog's background-thread write path alone, apart from the drain fix
  already made.
- If a site is in doubt, measure it (my memory notes: "instrument, don't guess").

## Goal 3: a readable report

- Issue::toString pads type/message/source to fixed columns (20/9/60/40). A message
  longer than 60 characters breaks the alignment: the catalog line ran into "Src:".
  Fix the layout so long messages keep the columns aligned (for example, wrap or
  overflow onto an indented continuation line, or move the detail after Obj:).
  prepareReport's collapse key depends on these columns: keep the two in step.
- Line up the "(repeated N times)" counts, and the section headers / rules, in
  WinnowReport.txt.
- Each issue's message should say WHAT failed and WHY (the error text), not just
  "failed".

## Goal 4: privacy

The report goes to the developer, or to a PUBLIC GitHub issue. Include only what
identifies a bug. Audit what can reach the logs:
- full paths (handled by "~" and hideFolders; check every path form, including
  Windows backslashes and UNC/network paths)
- file names (can contain people's names): keep them, but say so
- GPS coordinates, email/url/creator/copyright metadata, keyword text, collection
  and place names, camera serial numbers, user names in crash reports

Strip or never log what isn't needed. The dialog
(MW::reportIssue) and the help page must say plainly what the report contains, what
is hidden, that GitHub is public, and that the user can open WinnowReport.txt and
read it before sending.

## Project rules

- Follow CLAUDE.md and my memory notes: block comments, 90-character comment
  lines, US spelling.
- Every platform-specific branch has macOS, Windows and Linux implementations.
- Ask before building. For a single test target, use
  `cmake --build build/mac-debug --target tst_<name>`, then ctest at
  /Users/roryhill/Qt/Tools/CMake/CMake.app/Contents/bin/ctest. To check syntax
  without building, use compile_commands.json with -fsyntax-only.
- Update notes/Documentation.md 22.7.x in the same pass.
- Don't edit _ToDo.txt.
- Put temporary files in the scratchpad.
- Finish with a suggested commit summary.
