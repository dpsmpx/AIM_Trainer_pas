# Документация Aim Trainer

Что где. Правила работы — [`CLAUDE.md`](../CLAUDE.md); старшинство
документов — там же, «Источник истины».

| вопрос | документ |
|---|---|
| что решено и почему | [`direction/DECISIONS.md`](direction/DECISIONS.md) |
| какие этапы и в каком состоянии | [`ROADMAP/ROADMAP.md`](../ROADMAP/ROADMAP.md) |
| что не закончено, вопросы автору | [`TODO.md`](../TODO.md) |
| как устроена программа: модули, экраны, время, ввод, цели, счёт, файлы | [`ARCHITECTURE.md`](ARCHITECTURE.md) |
| как проверить правку на Windows | [`MANUAL_CHECKS.md`](MANUAL_CHECKS.md) |
| как собрать C++-версию у автора и в сессии, пресеты, тесты, эталон | [`BUILD.md`](BUILD.md) |
| перенос на C++: что где, сверка с эталоном, поведение, которое легко потерять | [`PORTING.md`](PORTING.md) |
| выбор библиотеки: замерщики ввода, как их прогнать и читать журналы | [`LIB_PROBES.md`](LIB_PROBES.md) |
| зачем правила и откуда | [`PROCESS.md`](PROCESS.md) |
| откуда код и на каком основании | [`PROVENANCE.md`](PROVENANCE.md) |
| вехи истории | [`CHANGELOG.md`](CHANGELOG.md) |
| аудит и ревью 13 октября 2026 — Pascal-версия | [`audits/PROJECT_AUDIT.md`](audits/PROJECT_AUDIT.md) |
| аудит 16 октября 2026 — C++-версия: интерфейс, производительность, смысл тренажёра, код | [`audits/CPP_AUDIT.md`](audits/CPP_AUDIT.md) |
| описание для игрока | [`README.md`](../README.md) — витрина, не источник истины (D-06) |

Проверка документов: `python3 tools/check_docs.py`; C++-версии —
`tools/check_cpp.sh` (D-25).
