# Автоматизация редактора

Скрипты, которыми проект проверялся и правился без ручных кликов. Они рассчитаны на папку `C:\ProjectsUE\_tools`
(пути внутри абсолютные), поэтому перед использованием скопируй содержимое этой папки туда.

Нужно: Unreal Engine 5.8, Visual Studio, плагин **Unreal MCP Server** в `The_battle_code/Plugins/UnrealMCPServer`
(он платный и в репозиторий не входит — см. `.gitignore`), Git Bash, Python 3.

| Скрипт | Что делает |
| --- | --- |
| `start_editor.sh` | Запускает редактор с проектом и ждёт, пока MCP-сервер ответит на порту 13579 |
| `rebuild.sh` | Закрывает редактор, собирает C++-плагины проекта, запускает редактор снова |
| `py.sh <файл.py>` | Выполняет Python-скрипт внутри редактора и печатает его вывод |
| `mcp.sh <инструмент> '<json>'` | Вызывает любой инструмент MCP-сервера |
| `smoke.sh <карта> <сек> <имя>` | Запускает карту в PIE, делает скриншот, считает ошибки блюпринтов |
| `autotest.sh <карта> <сек> <имя>` | То же, но отдаёт всех юнитов ИИ — бой играет сам; выводит HP юнитов и число намерений |
| `gshot.sh <имя>` | Скриншот запущенной игры вместе с интерфейсом: кадр `HighResShot` + слой UMG, нарисованный `KeepersInputSimulator.SaveViewportUI` (команда `shot showui` в 5.8 иногда роняет редактор) |
| `setup_menu_map.py` | Ставит в `L_MainMenu` сцену меню `KeepersMenuScene` вместо старого `BP_MainMenu` |
| `menu_page.py`, `menu_tune.py` | Открывают страницу главного меню и меняют ракурсы камеры в запущенной игре |
| `bpedit.py` | Мини-клиент для правки графов блюпринтов (узлы, пины, связи) |
| `edit_phases.py`, `edit_intents.py` | Правки графов, которые внесены в `BP_CombatSystem` и `BP_UnitAi` (повторный запуск ничего не меняет) |
| `vpshot.py` | Снимок игрового вьюпорта со всем интерфейсом, включая Slate (`KeepersCaptureLibrary.CaptureViewport`), в `_logs/v_<имя>.png`; имя берётся из `ui_shot.txt` |
| `squad_open.py`, `squad_probe.py` | Экран отряда в запущенном меню: `squad_arg.txt` = `open` или `test <Герой> <номер способности>`; печать фигур и камеры |
| `load_view.py`, `plain_shot.py` | Кадры для экранов загрузки: камера игрока ставится над полем (`load_yaw.txt`: угол, расстояние, высота, наклон), снимок `HighResShot` |
| `import_load.py` | Импорт `_logs/load/T_Load_*.png` в `/Game/Keepers/Loading` как UI-текстур |
| `load_preview.py`, `pause_toggle.py` | Показать экран загрузки карты из `load_level.txt`; открыть/закрыть меню паузы |
| `tt_lineup.py`, `capture.py`, `vshot.sh` | Ряды частей Toony Tiny RTS Set для выбора обликов, снимок редактора через SceneCapture |

Пример полной проверки боя:

```bash
/c/ProjectsUE/_tools/autotest.sh /Game/Maps/Slice/L_Slice_Hex 150 slice
```

Скриншоты и логи складываются в `C:\ProjectsUE\_logs`.
