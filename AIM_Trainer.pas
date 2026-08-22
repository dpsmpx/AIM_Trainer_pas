// ============================================================================
//  AIM_Trainer_AI.pas – Основной игровой модуль
//  Правки этой ревизии (относительно присланной версии v3):
//   - Устранена "потерянная" первая цель: сброс накопленной MillisecondsDelta
//     перед стартом игрового цикла. Раньше первый круг каждой игры почти
//     всегда молча не отрисовывался и забирал жизнь просто из-за времени,
//     проведённого в меню и на countdown (MillisecondsDelta копил дельту,
//     пока не вызывался, а вызывается он только в UpdateGame).
//   - Правый клик в Classic Mode больше не игнорируется молча — обработка
//     ЛКМ и ПКМ унифицирована через ProcessClick, GetHitInfo сам решает,
//     валидна ли кнопка для текущего режима/типа круга.
//   - "Accuracy" снова означает процент попаданий (Score/TotalShots), как
//     и должно быть. Добавлена отдельная метрика "Precision" — насколько
//     точно (по центру) приходятся именно попадания; раньше эти две вещи
//     были случайно смешаны в одной формуле.
//   - Разделены проверки TotalShots>0 / Score>0 при подсчёте avgReaction
//     на экране статистики забега — раньше при промахах без единого
//     попадания получался NaN/Infinity в промежуточном значении.
//   - LoadStats/SaveStats обёрнуты в try/except — повреждённый или занятый
//     другим процессом stats.txt больше не мешает игре запуститься.
//   - Добавлено мягкое ограничение частоты кадров (~240 FPS) во всех
//     экранах — циклы были ничем не ограничены и могли впустую грузить
//     ядро CPU. Порог выбран с большим запасом, чтобы не влиять на
//     точность замера реакции (использует отдельный от MillisecondsDelta
//     таймер, чтобы не трогать тайминг реакции).
//   - Добавлен SetWindowIsFixedSize(true) — вся раскладка UI жёстко
//     завязана на 800x600, ресайз окна её ломал.
//   - TCircle.Spawn/Update/Render/GetHitInfo больше не принимают параметры,
//     дублирующие поля Game (MaxRadius/MaxCTime/CCTime/GameMode) — методы
//     и так уже читали Game напрямую местами, теперь это последовательно
//     everywhere, сигнатуры короче и риска рассинхронизации между
//     параметром и Game.* больше нет.
//   - Убран мёртвый флаг Game.IsPlayTime — его роль полностью выполняет
//     Screen (TCircle/UpdateGame вызываются только пока Screen = scGame).
//   - Клики ЛКМ/ПКМ обрабатываются одной процедурой ProcessClick вместо
//     двух копий одного и того же кода.
//   - В Dual Mode добавлена компактная подсказка "какой цвет — какая
//     кнопка" в левом верхнем углу; под неё в Dual Mode дополнительно
//     резервируется место, чтобы круг не мог заспавниться под ней.
//   - Lives на HUD не может уйти в отрицательные значения даже теоретически.
//
//  Дополнительно: сюда влита плашка выбора режима из присланной версии v4
//  (она была написана поверх старого, ещё не исправленного v3 — все правки
//  выше в ней отсутствовали, плюс у самой плашки были свои проблемы):
//   - Не хватало ClearWindow в начале RenderMainMenu — экран не очищался,
//     меню "смазывалось" от кадра к кадру.
//   - DrawParticles вызывался дважды за кадр (в начале и в конце) — вдвое
//     лишняя нагрузка и задвоенный, слишком яркий эффект от наложения
//     полупрозрачных частиц самих на себя.
//   - MenuAnimTimer рос на фиксированные +16 за итерацию цикла, а не на
//     реальное время — скорость анимации плавала в зависимости от того,
//     насколько быстро крутится цикл на конкретном железе (тем более что
//     кап FPS раньше отсутствовал вообще). Теперь считается от Milliseconds.
//   - InitModes вызывался из InitGameState, то есть на каждый забег заново —
//     помимо лишней работы это сбрасывало выбранный в меню режим и таймер
//     анимации при каждом переходе Countdown → Game. Перенесено в точку
//     однократной инициализации программы.
//   - Список режимов расширили до 4 (Classic, Dual, Tracking, Flick), но
//     реальная игровая механика (TCircle/UpdateGame) по-прежнему знает
//     только Classic и Dual — Tracking и Flick запускали игру, которая
//     молча вела себя как Classic, вообще не совпадая с описанием на
//     плашке. Добавлено поле TMenuMode.Implemented: превью и описание для
//     всех четырёх работают, но запуск с плашки заблокирован для тех двух,
//     где механики не существует (показывается "СКОРО"), пока это не
//     станет отдельной задачей.
//   - Раскладка кнопок списка режимов накладывалась на блок описания под
//     ней примерно на 28px при 4 режимах — пересчитаны отступы.
//   - `const MAX_CHARS = 32;` был объявлен внутри секции var — вынесен в
//     свою секцию const перед var, как положено.
//   - В RenderMainMenu версии v4 пропала нижняя статистическая панель
//     (Best Score/Accuracy/Avg/Games) и индикатор Effects ON/OFF — оба
//     восстановлены, чтобы не терять то, что уже работало.
//   - Старая раскладка кнопок (TButton/Buttons/InitUI/CreateButton) удалена
//     как мёртвый код: новую плашку она больше не подключалась и никогда
//     не инициализировалась в v4 (SetLength для Buttons не вызывался).
//
//  Этап 1 (по заявке — мелкое и низкорисковое):
//   - Горячие клавиши R/M/P. R — рестарт текущего режима без захода в меню
//     (незавершённый забег в статистику не идёт); M — сразу в меню (забег
//     считается завершённым и сохраняется, как при Esc); P — пауза
//     (UpdateGame не вызывается, но кадр продолжает рендериться и рисуется
//     затемнение поверх). При снятии паузы дельта MillisecondsDelta
//     сбрасывается тем же способом, что и после countdown — иначе время
//     на паузе утекло бы в тайминг следующей цели ровно как баг с первым
//     кругом, который чинили раньше. R и M работают и на экране Game Over,
//     M — ещё и во время countdown.
//   - Логика подсчёта/сохранения статистики забега вынесена из GameLoop в
//     отдельную FinishGameAndSaveStats — раньше была только одна точка
//     выхода (Fail), теперь их две (Fail и M), дублировать код не стали.
//   - Кнопка "[?] Справка" внизу слева на главном меню (симметрично
//     Effects ON/OFF справа) открывает панель с управлением и объяснением
//     Accuracy/Precision; закрывается кликом или Esc.
//   - Автосохранение — уже было (SaveStats вызывается в конце каждого
//     забега), отдельно ничего добавлять не пришлось, только закрепили
//     это через FinishGameAndSaveStats как единую точку.
//   - Кнопки режимов на плашке теперь появляются каскадом (фейд + сдвиг
//     снизу вверх, полёт ~400мс, старт кнопки i — через i*80мс после входа
//     в меню) и мягко пульсируют рамкой при наведении.
//
//  Этап 2 (среднее, но самостоятельное):
//   - Звук через System.Console.Beep (в GraphABC своего звука нет, это
//     подтверждённый рабочий способ). Beep блокирует поток на время
//     сигнала, поэтому у попадания/промаха он специально короткий
//     (30-40мс) — тайминг реакции важнее звука. У Game Over звук длиннее
//     (220мс) — там уже не идёт активный отсчёт реакции. Отключается в
//     настройках.
//   - Панель настроек ("[⚙] Настройки" рядом со справкой): размер круга,
//     время жизни цели, число жизней, звук вкл/выкл, плюс сброс на
//     умолчания. Применяется со следующей игры (InitGameState теперь
//     берёт MaxRadius/MaxCTime/MaxLoses из Settings, а не из хардкода).
//     Сохраняется в тот же stats.txt новыми ключами Set*.
//   - Бэкап stats.txt при повреждении: если LoadStats падает на разборе
//     файла, повреждённая версия сначала копируется в
//     stats_corrupt_backup.txt (через Assign/Reset/Rewrite — те же
//     примитивы, что уже используются, а не непроверенный CopyFile), и
//     только потом всё сбрасывается на значения по умолчанию.
//   - Переменный размер целей: у каждого круга свой SizeFactor
//     (0.65..1.35 от базового размера), меньше круг — больше очков.
//     Это разделило две раньше совпадавшие вещи: Game.Score (очки) и
//     Game.Hits (чистое число попаданий). Score теперь используется
//     только для отображения и Best Score; Accuracy/Avg/Precision везде
//     считаются по Hits, как и раньше по смыслу.
//   - Зоны точности: два полупрозрачных кольца на 50%/25% текущего
//     радиуса цели — ориентир, куда целиться ради максимальной Accuracy
//     конкретного попадания (формула та же, что в GetHitInfo).
//   - Частицы при попадании (вспышка + кольцевая волна + 8 осколков +
//     всплывающий "+N") и след курсора во время игры — через общий
//     кольцевой буфер эффектов (100 слотов), без ручной очистки: слот
//     истёкшего эффекта просто перезаписывается следующим.
//
//  Этап 3 (критерии для достижений/тем выбраны мной — список ниже,
//  список и формулировки можно поправить одним словом при следующей
//  правке):
//   - Таблица лидеров: топ-10 по очкам отдельно для Classic и Dual,
//     хранится в том же stats.txt (ключи LBCount*/LB*), обновляется в
//     конце каждого забега вставкой с сохранением сортировки.
//   - Достижения (8 шт, критерии мои): Первая игра, В яблочко (точность
//     ≥95% на попадании), Молниеносно (реакция <150мс), Комбо x100,
//     Тысяча (1000+ очков за игру), Снайпер (20+ выстрелов и 90%+
//     точности), Ветеран (1000 игр), Мастер Dual (500+ очков в Dual).
//     Проверяются в ProcessClick (посекундные) и FinishGameAndSaveStats
//     (по итогу игры); разблокировка сразу сохраняется и на 3 секунды
//     показывает всплывающее уведомление (RenderToast).
//   - Туториал первого запуска: 4 экрана, показывается один раз, пока
//     Stats.TutorialSeen=false; клик или Esc листает/пропускает.
//   - Комбо-система: Game.Combo растёт на попаданиях, сбрасывается на
//     промахе; множитель очков x1.5 с 5 подряд, x2 с 10 подряд.
//   - Темы оформления: Тёмная (как было) / Неон / Минимализм — все три
//     нарочно оставлены на тёмном фоне. Настоящая светлая тема потребовала
//     бы переписать Font.Color в каждом месте файла (их десятки) без
//     возможности проверить компиляцией — решил не рисковать. Переключение
//     в настройках, применяется сразу.
//   - Слоумо на точное попадание в центр (≥95%) сделано как визуальная
//     вспышка на весь экран (тип эффекта 4), а не как реальное замедление
//     FrameTime/TryTime — те напрямую определяют точность замера реакции,
//     этим не рискуем ради красивого эффекта.
//   - Бомбы: с 15% шансом (кроме самой первой цели забега) вместо обычной
//     цели спавнится бомба — тёмный круг с белым крестом. Клик по ней
//     (любой кнопкой) — мгновенный Game.Fail с отдельным сообщением на
//     Game Over; не тронул за её время жизни — исчезает без штрафа
//     (TCircle.Update пропускает Inc(Loses) для бомб).
//
//  Не тестировалось компиляцией (нет PascalABC.NET в текущем окружении) —
//  перед использованием прогони через компилятор. Часть API у плашки
//  (Color.FromArgb, SetBrushColor/SetFontStyle и т.п., TextOut/TextWidth,
//  Circle) проверена по официальным примерам PascalABC.NET веб-поиском,
//  но это не замена реальной компиляции.
// ============================================================================

uses GraphABC, Base, Control_AI, AimCircle;

type
  TScreen = (scMainMenu, scCountdown, scGame, scGameOver);
  // TGameMode, THitInfo, TCircle (+TCircleContext) теперь в отдельном
  // модуле AimCircle.pas — вынесены оттуда согласно пункту "рефакторинг
  // структур" (см. заголовок). Основной файл только собирает контекст из
  // Game и передаёт его методам TCircle явным параметром.

  // Описание одного режима в главном меню: подпись, текст, цвета анимации
  // на плашке и то, какой реальный GameMode он запускает
  TMenuMode = record
    Name: string;
    Description: string;
    ButtonColor: Color;
    CircleColor: Color;
    CircleColor2: Color;
    AnimType: integer; // 0=появление, 1=чередование (Dual), 2=движение, 3=мелькание
    GameMode: TGameMode;
    Implemented: boolean; // false = только превью в меню, запуск игры заблокирован
  end;

  TGameState = record
    Score: integer;      // очки (переменные, зависят от размера цели)
    Hits: integer;        // сколько раз реально попали — отдельно от Score,
                           // т.к. Score больше не равен числу попаданий
    TotalShots: integer;
    Loses: integer;
    WrongClicks: integer;
    TotalReactionTime: integer;
    TotalPrecisionSum: real;
    MaxLoses: integer;
    Fail: boolean;
    DiedToBomb: boolean; // для отдельного сообщения на экране Game Over
    TryTime: integer;
    Circles: array of TCircle;
    CircleCount: integer;
    MaxRadius: integer;
    MaxCTime: integer;
    CCTime: integer;
    ReactionHistory: array[0..49] of integer;
    ReactionCount: integer;
    ReactionIndex: integer;
    FrameTime: integer;
    LastTime: integer;
    FPS: integer;
    tempFPS: integer;
    GameMode: TGameMode;
    CountdownValue: integer;
    CountdownStartTime: integer;
    Paused: boolean;
    Combo: integer;     // серия попаданий подряд без промаха
    MaxCombo: integer;  // лучшая серия за этот забег
  end;

  TParticle = record
    X, Y: real;
    DX, DY: real;
    procedure Init(W, H: integer);
    procedure Update(W, H: integer);
    procedure Draw;
  end;

  TStats = record
    BestScore: integer;
    BestAccuracy: real;
    BestPrecision: real;
    BestAvgReaction: real;
    GamesPlayed: integer;
    TutorialSeen: boolean;
  end;

  // Настраиваемые пользователем параметры (меню настроек), отдельно от
  // TStats — это не "лучшие результаты", а конфигурация на будущие игры
  TSettings = record
    CircleSize: integer;   // базовый радиус цели (до случайного SizeFactor)
    LifetimeMs: integer;   // сколько цель живёт целиком (было хардкодом 1000)
    Lives: integer;        // сколько жизней даётся на забег
    SoundEnabled: boolean;
  end;

  // Короткоживущий визуальный эффект (вспышка/волна/осколок/текст очков/
  // полноэкранная вспышка на очень точное попадание)
  TEffect = record
    Active: boolean;
    EType: integer; // 0=вспышка 1=волна 2=осколок 3=текст 4=экран-white 5=экран-red
    X, Y: real;
    VX, VY: real;
    StartTime: integer;
    Duration: integer;
    Col: Color;
    Text: string;
  end;

  TLeaderboardEntry = record
    Score: integer;
    Accuracy: real;
  end;

  TTheme = record
    Name: string;
    BgColor: Color;
    AccentColor: Color;
    Circle1: Color;
    Circle2: Color;
  end;

var
  W := 800;
  H := 600;
  CircleColors: array of Color;
  Game: TGameState;
  Screen: TScreen;
  Stats: TStats;
  Settings: TSettings;
  SavedReactionHistory: array[0..49] of integer;
  SavedReactionCount: integer;
  SavedReactionIndex: integer;
  ShowMainMenuEffects: boolean;
  LastFrameTick: integer;

  // ----- Профили -----
  CurrentProfile: integer; // 1..MAX_PROFILES; у каждого свой файл всего
  ShowProfiles: boolean;

  // ----- Темы оформления -----
  Themes: array[0..2] of TTheme;
  CurrentTheme: integer;

  // ----- Таблица лидеров (по режимам: 0=Classic,1=Dual,2=Tracking,3=Flick) -----
  Leaderboard: array[0..3] of array[0..9] of TLeaderboardEntry;
  LeaderboardCount: array[0..3] of integer;
  ShowLeaderboard: boolean;

  // ----- Достижения -----
  AchievementUnlocked: array[0..7] of boolean;
  AchievementNames: array[0..7] of string;
  AchievementDescs: array[0..7] of string;
  ShowAchievements: boolean;

  // ----- Всплывающее уведомление (достижение получено) -----
  ToastText: string;
  ToastStartTime: integer;
  ToastActive: boolean;

  // ----- Туториал первого запуска -----
  ShowTutorial: boolean;
  TutorialStep: integer;
  TutorialTexts: array[0..3] of string;

  // ----- Плашка выбора режима в главном меню -----
  Modes: array[0..9] of TMenuMode;
  ModeCount: integer;
  SelectedModeIndex: integer;
  MenuAnimTimer: integer;
  LastMenuAnimTick: integer;
  MenuEnterTick: integer;  // для анимации появления кнопок (см. DrawModeButtons)
  ShowHelp: boolean;       // показана ли справка поверх меню
  ShowSettings: boolean;   // показана ли панель настроек поверх меню

  // ----- Эффекты попадания и след курсора (только во время игры) -----
  Effects: array[0..99] of TEffect;
  NextEffectSlot: integer;
  TrailPoints: array[0..11] of Point;
  TrailFilled: array[0..11] of boolean;
  TrailNext: integer;

  // ----- Тепловая карта промахов (текущий забег, сбрасывается в InitGameState) -----
  MissHeatmap: array[0..19] of array[0..14] of integer;

  // ----- История сессий (из sessions.csv текущего профиля) -----
  ShowHistory: boolean;
  LoadedSessionCount: integer;
  LoadedSessionHour: array[0..499] of integer;
  LoadedSessionScore: array[0..499] of integer;
  LoadedSessionAvgReaction: array[0..499] of real;

const
  PARTICLE_COUNT = 100;
  PARTICLE_SPEED = 1.0;
  LINE_DIST = 100.0;
  TARGET_FPS = 240;
  MIN_FRAME_MS = 1000 div TARGET_FPS;

  MAX_EFFECTS = 100;
  TRAIL_LENGTH = 12;

  // Горячие клавиши. Используем сырые виртуальные коды вместо VK_R/VK_M/VK_P,
  // потому что для букв они не встретились ни в одном официальном примере —
  // а это стандартные Win32 virtual-key коды ('R'=82,'M'=77,'P'=80), они не
  // зависят от того, объявлены ли в GraphABC именно такие константы.
  KEY_R = 82;
  KEY_M = 77;
  KEY_P = 80;

  // Плашка предпросмотра режима (левая половина верхней части меню)
  PLATE_X = 50;   PLATE_Y = 40;
  PLATE_W = 330;  PLATE_H = 240;

  // Список режимов (правая половина). Высота/отступ кнопок подобраны так,
  // чтобы 4 кнопки не наезжали на блок описания под ними (в исходнике
  // v4 наезжали на ~28px).
  BTNS_X = 420;   BTNS_Y = 40;
  BTNS_W = 330;   BTNS_H = 34;
  BTNS_GAP = 8;

  DESC_X = 420;   DESC_Y = 214;
  DESC_W = 330;   DESC_H = 76;

  // Границы значений в панели настроек
  MIN_CIRCLE_SIZE = 20;  MAX_CIRCLE_SIZE = 60;
  MIN_LIFETIME = 500;    MAX_LIFETIME = 2000;  LIFETIME_STEP = 100;
  MIN_LIVES = 5;         MAX_LIVES = 30;

  // Диапазон случайного масштаба цели: меньше — очков больше (см. ProcessClick)
  MIN_SIZE_FACTOR = 0.65;
  MAX_SIZE_FACTOR = 1.35;

  // Панель настроек
  // Панель настроек — выросла на одну строку (тема оформления), поэтому
  // выше и кнопка сброса теперь ниже (row 5 вместо row 4)
  SET_PANEL_W = 420;  SET_PANEL_H = 390;
  SET_ROW_H = 40;
  SET_BTN_W = 34;

  // Шанс, что вместо обычной цели заспавнится бомба (после первой цели
  // забега — см. TCircle.Spawn)
  BOMB_CHANCE = 0.15;
  BOMB_ACCURACY_THRESHOLD = 0.95; // порог "точно в яблочко" — для достижения и слоумо-вспышки

  MAX_PROFILES = 4;
  HEATMAP_COLS = 20;
  HEATMAP_ROWS = 15;
  MAX_SESSIONS_LOADED = 500; // с запасом на реальную историю игр одного профиля

var
  Particles: array[0..PARTICLE_COUNT-1] of TParticle;

// Мягкое ограничение FPS. Порог (240) намного выше герцовки любого монитора
// и не мешает измерению реакции, но не даёт циклу впустую жечь ядро CPU.
// Специально использует Milliseconds (абсолютные часы), а не
// MillisecondsDelta — тот зарезервирован только под тайминг геймплея,
// чтобы вызовы отсюда никак не повлияли на дельту кадра в UpdateGame.
procedure LimitFrameRate;
var
  elapsed: integer;
begin
  elapsed := Milliseconds - LastFrameTick;
  if (elapsed >= 0) and (elapsed < MIN_FRAME_MS) then
    Sleep(MIN_FRAME_MS - elapsed);
  LastFrameTick := Milliseconds;
end;

// ----- Загрузка/сохранение статистики и настроек -----
// Профили: у каждого свой набор файлов (этап 4, п.22). Переключение —
// это просто смена CurrentProfile + повторный LoadStats на новое имя.
function StatsFileName: string;
begin
  Result := 'profile' + CurrentProfile.ToString + '_stats.ini';
end;

function SessionsFileName: string;
begin
  Result := 'profile' + CurrentProfile.ToString + '_sessions.csv';
end;

function BackupFileName: string;
begin
  Result := 'profile' + CurrentProfile.ToString + '_corrupt_backup.txt';
end;

procedure SetDefaultSettings;
begin
  Settings.CircleSize := 40;
  Settings.LifetimeMs := 1000;
  Settings.Lives := 15;
  Settings.SoundEnabled := true;
end;

// Копирует повреждённый файл статистики в отдельный файл перед тем, как
// затереть его значениями по умолчанию — чтобы не терять данные молча.
// Использует только Assign/Reset/Rewrite/Readln/Writeln/Close — те же
// примитивы, что уже работают в LoadStats/SaveStats, вместо непроверенной
// CopyFile.
procedure BackupCorruptStatsFile;
var
  fIn, fOut: Text;
  line: string;
begin
  try
    Assign(fIn, StatsFileName);
    Reset(fIn);
    try
      Assign(fOut, BackupFileName);
      Rewrite(fOut);
      try
        while not Eof(fIn) do
        begin
          Readln(fIn, line);
          Writeln(fOut, line);
        end;
      finally
        Close(fOut);
      end;
    finally
      Close(fIn);
    end;
  except
    // Не получилось сделать бэкап (например, файл уже нечитаем целиком) —
    // не критично, продолжаем со сбросом на значения по умолчанию
  end;
end;

procedure SaveStats;
var
  f: Text;
  i, m, rk: integer;
begin
  try
    Assign(f, StatsFileName);
    Rewrite(f);
    try
      // Формат — INI-подобный: секции для читаемости человеком, но парсит
      // их LoadStats тем же построчным key=value разбором, что и раньше —
      // строки-заголовки секций просто не содержат '=' и пропускаются его
      // проверкой Length(parts)=2, отдельный код под секции не понадобился.
      Writeln(f, '[Stats]');
      Writeln(f, 'BestScore=' + Stats.BestScore.ToString);
      Writeln(f, 'BestAccuracy=' + Stats.BestAccuracy.ToString);
      Writeln(f, 'BestPrecision=' + Stats.BestPrecision.ToString);
      Writeln(f, 'BestAvgReaction=' + Stats.BestAvgReaction.ToString);
      Writeln(f, 'GamesPlayed=' + Stats.GamesPlayed.ToString);
      Writeln(f, 'TutorialSeen=' + Stats.TutorialSeen.ToString);

      Writeln(f, '[Settings]');
      Writeln(f, 'ShowMainMenuEffects=' + ShowMainMenuEffects.ToString);
      Writeln(f, 'SetCircleSize=' + Settings.CircleSize.ToString);
      Writeln(f, 'SetLifetimeMs=' + Settings.LifetimeMs.ToString);
      Writeln(f, 'SetLives=' + Settings.Lives.ToString);
      Writeln(f, 'SetSoundEnabled=' + Settings.SoundEnabled.ToString);
      Writeln(f, 'CurrentTheme=' + CurrentTheme.ToString);

      Writeln(f, '[ReactionHistory]');
      Writeln(f, 'ReactionCount=' + SavedReactionCount.ToString);
      Writeln(f, 'ReactionIndex=' + SavedReactionIndex.ToString);
      for i := 0 to 49 do
        Writeln(f, 'Reaction' + i.ToString + '=' + SavedReactionHistory[i].ToString);

      Writeln(f, '[Achievements]');
      for i := 0 to 7 do
        Writeln(f, 'Achv' + i.ToString + '=' + AchievementUnlocked[i].ToString);

      Writeln(f, '[Leaderboard]');
      for m := 0 to 3 do
      begin
        Writeln(f, 'LBCount' + m.ToString + '=' + LeaderboardCount[m].ToString);
        for rk := 0 to LeaderboardCount[m] - 1 do
        begin
          Writeln(f, 'LB' + m.ToString + rk.ToString + 'S=' + Leaderboard[m][rk].Score.ToString);
          Writeln(f, 'LB' + m.ToString + rk.ToString + 'A=' + Leaderboard[m][rk].Accuracy.ToString);
        end;
      end;
    finally
      Close(f);
    end;
  except
    // Не удалось сохранить статистику (например, файл занят) — не
    // критично для игры, просто пропускаем сохранение в этот раз
  end;
end;

procedure LoadStats;
var
  f: Text;
  line: string;
  parts: array of string;
  i, idx, val, m, rk, ai: integer;
  loaded: boolean;
begin
  // Инициализация значений по умолчанию
  Stats.BestScore := 0;
  Stats.BestAccuracy := 0;
  Stats.BestPrecision := 0;
  Stats.BestAvgReaction := 0;
  Stats.GamesPlayed := 0;
  Stats.TutorialSeen := false;
  SavedReactionCount := 0;
  SavedReactionIndex := 0;
  for i := 0 to 49 do SavedReactionHistory[i] := 0;
  SetDefaultSettings;
  CurrentTheme := 0;
  for m := 0 to 3 do LeaderboardCount[m] := 0;
  for i := 0 to 7 do AchievementUnlocked[i] := false;
  loaded := false;

  // Если файл существует – пытаемся загрузить
  if FileExists(StatsFileName) then
  begin
    try
      Assign(f, StatsFileName);
      Reset(f);
      try
        while not Eof(f) do
        begin
          Readln(f, line);
          parts := line.Split('=');
          if Length(parts) = 2 then
          begin
            if parts[0] = 'BestScore' then Stats.BestScore := StrToInt(parts[1])
            else if parts[0] = 'BestAccuracy' then Stats.BestAccuracy := StrToFloat(parts[1])
            else if parts[0] = 'BestPrecision' then Stats.BestPrecision := StrToFloat(parts[1])
            else if parts[0] = 'BestAvgReaction' then Stats.BestAvgReaction := StrToFloat(parts[1])
            else if parts[0] = 'GamesPlayed' then Stats.GamesPlayed := StrToInt(parts[1])
            else if parts[0] = 'TutorialSeen' then Stats.TutorialSeen := (parts[1] = 'True')
            else if parts[0] = 'ReactionCount' then SavedReactionCount := StrToInt(parts[1])
            else if parts[0] = 'ReactionIndex' then SavedReactionIndex := StrToInt(parts[1])
            else if parts[0] = 'ShowMainMenuEffects' then ShowMainMenuEffects := (parts[1] = 'True')
            else if parts[0] = 'SetCircleSize' then Settings.CircleSize := StrToInt(parts[1])
            else if parts[0] = 'SetLifetimeMs' then Settings.LifetimeMs := StrToInt(parts[1])
            else if parts[0] = 'SetLives' then Settings.Lives := StrToInt(parts[1])
            else if parts[0] = 'SetSoundEnabled' then Settings.SoundEnabled := (parts[1] = 'True')
            else if parts[0] = 'CurrentTheme' then CurrentTheme := StrToInt(parts[1])
            else if Copy(parts[0], 1, 7) = 'LBCount' then
            begin
              m := StrToInt(Copy(parts[0], 8, Length(parts[0]) - 7));
              if (m >= 0) and (m <= 3) then LeaderboardCount[m] := StrToInt(parts[1]);
            end
            else if (Length(parts[0]) = 5) and (Copy(parts[0], 1, 2) = 'LB') then
            begin
              m := StrToInt(Copy(parts[0], 3, 1));
              rk := StrToInt(Copy(parts[0], 4, 1));
              if (m >= 0) and (m <= 3) and (rk >= 0) and (rk <= 9) then
              begin
                if parts[0][5] = 'S' then Leaderboard[m][rk].Score := StrToInt(parts[1])
                else if parts[0][5] = 'A' then Leaderboard[m][rk].Accuracy := StrToFloat(parts[1]);
              end;
            end
            else if Copy(parts[0], 1, 4) = 'Achv' then
            begin
              ai := StrToInt(Copy(parts[0], 5, Length(parts[0]) - 4));
              if (ai >= 0) and (ai <= 7) then AchievementUnlocked[ai] := (parts[1] = 'True');
            end
            else if Copy(parts[0], 1, 8) = 'Reaction' then
            begin
              idx := StrToInt(Copy(parts[0], 9, Length(parts[0])-8));
              val := StrToInt(parts[1]);
              if (idx >= 0) and (idx < 50) then
                SavedReactionHistory[idx] := val;
            end;
          end;
        end;
        loaded := true; // чтение прошло без ошибок
      finally
        Close(f);
      end;
    except
      // Ошибка при чтении – создаём бэкап и сбрасываем всё на дефолты
      BackupCorruptStatsFile;
      // Сброс значений уже сделан в начале, но нужно убедиться, что они дефолтные
      // (они уже установлены, но могли быть частично изменены до исключения)
      Stats.BestScore := 0;
      Stats.BestAccuracy := 0;
      Stats.BestPrecision := 0;
      Stats.BestAvgReaction := 0;
      Stats.GamesPlayed := 0;
      Stats.TutorialSeen := false;
      SavedReactionCount := 0;
      SavedReactionIndex := 0;
      for i := 0 to 49 do SavedReactionHistory[i] := 0;
      SetDefaultSettings;
      CurrentTheme := 0;
      for m := 0 to 3 do LeaderboardCount[m] := 0;
      for i := 0 to 7 do AchievementUnlocked[i] := false;
      loaded := false;
    end;
  end;

  // Защита от некорректных значений (если файл был повреждён частично)
  if (Settings.CircleSize < MIN_CIRCLE_SIZE) or (Settings.CircleSize > MAX_CIRCLE_SIZE) then
    Settings.CircleSize := 40;
  if (Settings.LifetimeMs < MIN_LIFETIME) or (Settings.LifetimeMs > MAX_LIFETIME) then
    Settings.LifetimeMs := 1000;
  if (Settings.Lives < MIN_LIVES) or (Settings.Lives > MAX_LIVES) then
    Settings.Lives := 15;
  if (CurrentTheme < 0) or (CurrentTheme > 2) then
    CurrentTheme := 0;

  // Если файл не существовал или был повреждён – создаём новый с дефолтными значениями
  if not loaded then
    SaveStats;
end;

// ----- Частицы -----
procedure TParticle.Init(W, H: integer);
begin
  X := Random * W;
  Y := Random * H;
  var angle := Random * 2 * Pi;
  DX := Cos(angle) * PARTICLE_SPEED;
  DY := Sin(angle) * PARTICLE_SPEED;
end;

procedure TParticle.Update(W, H: integer);
begin
  X := X + DX;
  Y := Y + DY;
  if (X < 0) or (X > W) then DX := -DX;
  if (Y < 0) or (Y > H) then DY := -DY;
  if X < 0 then X := 0;
  if X > W then X := W;
  if Y < 0 then Y := 0;
  if Y > H then Y := H;
end;

procedure TParticle.Draw;
begin
  Brush.Color := ARGB(60, 200, 200, 255);
  FillCircle(Round(X), Round(Y), 3);
end;

procedure InitParticles;
var
  i: integer;
begin
  for i := 0 to PARTICLE_COUNT-1 do
    Particles[i].Init(W, H);
end;

procedure DrawParticles;
var
  i, j: integer;
  dx, dy, dist: real;
  mousePos: Point;
begin
  // Линии от курсора к частицам (только в главном меню, если включено)
  if (Screen = scMainMenu) and ShowMainMenuEffects then
  begin
    mousePos := GetMousePos;
    for i := 0 to PARTICLE_COUNT-1 do
    begin
      dx := Particles[i].X - mousePos.X;
      dy := Particles[i].Y - mousePos.Y;
      dist := Sqrt(dx*dx + dy*dy);
      if dist < LINE_DIST * 2 then
      begin
        var alpha := Round(120 * (1 - dist / LINE_DIST));
        Pen.Color := ARGB(alpha, 100, 200, 255);
        Pen.Width := 1;
        Line(Round(Particles[i].X), Round(Particles[i].Y), mousePos.X, mousePos.Y);
      end;
    end;
  end;

  // Линии между частицами
  for i := 0 to PARTICLE_COUNT-2 do
    for j := i+1 to PARTICLE_COUNT-1 do
    begin
      dx := Particles[i].X - Particles[j].X;
      dy := Particles[i].Y - Particles[j].Y;
      dist := Sqrt(dx*dx + dy*dy);
      if dist < LINE_DIST then
      begin
        var alpha := Round(120 * (1 - dist / LINE_DIST));
        Pen.Color := ARGB(alpha, 100, 200, 255);
        Pen.Width := 1;
        Line(Round(Particles[i].X), Round(Particles[i].Y),
             Round(Particles[j].X), Round(Particles[j].Y));
      end;
    end;
  // Точки
  for i := 0 to PARTICLE_COUNT-1 do
    Particles[i].Draw;
end;

// ----- TCircle -----
// Сама реализация TCircle теперь в модуле AimCircle.pas (этап 4, п. 26).
// Здесь только context-builder — собирает TCircleContext из Game одной
// строкой перед каждым вызовом методов TCircle.
function MakeCircleContext: TCircleContext;
begin
  Result.W := W;
  Result.H := H;
  Result.MaxRadius := Game.MaxRadius;
  Result.MaxCTime := Game.MaxCTime;
  Result.CCTime := Game.CCTime;
  Result.TryTime := Game.TryTime;
  Result.GameMode := Game.GameMode;
end;

// ----- Режимы для плашки в главном меню -----
// Вызывается ОДИН раз при старте программы (см. главный блок), а не при
// каждом запуске игры: описания режимов — статические данные, пересоздавать
// их на каждый забег незачем, а раньше это ещё и сбрасывало выбранный в
// меню режим и таймер анимации на каждый Countdown→Game переход.
procedure InitModes;
begin
  ModeCount := 0;

  with Modes[ModeCount] do
  begin
    Name := 'Classic Mode';
    Description := 'Кликай ЛКМ по появляющимся кругам. Чем ближе к центру — тем выше точность!';
    ButtonColor := RGB(70, 130, 180);
    CircleColor := RGB(30, 144, 255);
    CircleColor2 := RGB(30, 144, 255);
    AnimType := 0;
    GameMode := gmClassic;
    Implemented := true;
  end;
  Inc(ModeCount);

  with Modes[ModeCount] do
  begin
    Name := 'Dual Mode';
    Description := 'Синие круги — ЛКМ, красные — ПКМ. Проверь обе руки!';
    ButtonColor := RGB(205, 92, 92);
    CircleColor := RGB(30, 144, 255);
    CircleColor2 := RGB(220, 20, 60);
    AnimType := 1;
    GameMode := gmDual;
    Implemented := true;
  end;
  Inc(ModeCount);

  // Ниже — два режима "на витрине": превью и описание уже работают,
  // но самой игровой механики под них пока нет (TCircle её не умеет),
  // поэтому запуск с плашки для них заблокирован (Implemented = false)
  // до тех пор, пока механика не будет реализована отдельной задачей.
  // Этап 4: Tracking и Flick теперь реально реализованы (см. AimCircle.pas
  // и UpdateGame) — заглушки сняты, Implemented = true у обоих.
  with Modes[ModeCount] do
  begin
    Name := 'Tracking';
    Description := 'Удерживай курсор на движущемся круге заданное время. Не отрывайся!';
    ButtonColor := RGB(60, 179, 113);
    CircleColor := RGB(46, 139, 87);
    CircleColor2 := RGB(46, 139, 87);
    AnimType := 2;
    GameMode := gmTracking;
    Implemented := true;
  end;
  Inc(ModeCount);

  with Modes[ModeCount] do
  begin
    Name := 'Flick Shot';
    Description := 'Круг появляется на мгновение. Тренируй скорость реакции!';
    ButtonColor := RGB(255, 140, 0);
    CircleColor := RGB(255, 165, 0);
    CircleColor2 := RGB(255, 165, 0);
    AnimType := 3;
    GameMode := gmFlick;
    Implemented := true;
  end;
  Inc(ModeCount);

  SelectedModeIndex := 0;
  MenuAnimTimer := 0;
  LastMenuAnimTick := Milliseconds;
end;

// ----- Инициализация игры -----
procedure InitGameState;
var
  i: integer;
begin
  with Game do
  begin
    Score := 0;
    Hits := 0;
    TotalShots := 0;
    Loses := 0;
    WrongClicks := 0;
    TotalReactionTime := 0;
    TotalPrecisionSum := 0;
    MaxLoses := Settings.Lives;
    Fail := false;
    DiedToBomb := false;
    Paused := false;
    TryTime := 0;
    MaxRadius := Settings.CircleSize;
    MaxCTime := Settings.LifetimeMs;
    CCTime := MaxCTime div 2;
    if GameMode = gmFlick then
    begin
      // Flick: короткая фиксированная жизнь цели независимо от настройки —
      // внезапное появление и есть суть режима, обычная формула роста/
      // сжатия (в AimCircle) с таким MaxCTime сама даёт нужный эффект
      MaxCTime := 400;
      CCTime := MaxCTime div 2;
    end;
    CircleCount := 1;
    SetLength(Circles, CircleCount);
    FrameTime := 0;
    LastTime := 1000;
    FPS := 0;
    tempFPS := 0;
    ReactionCount := 0;
    ReactionIndex := 0;
    Combo := 0;
    MaxCombo := 0;
    for i := 0 to 49 do ReactionHistory[i] := 0;
    Circles[0].Spawn(Circles[0].X, Circles[0].Y, MakeCircleContext);
  end;

  for i := 0 to MAX_EFFECTS - 1 do Effects[i].Active := false;
  NextEffectSlot := 0;
  for i := 0 to TRAIL_LENGTH - 1 do TrailFilled[i] := false;
  TrailNext := 0;

  for var cx := 0 to HEATMAP_COLS - 1 do
    for var cy := 0 to HEATMAP_ROWS - 1 do
      MissHeatmap[cx][cy] := 0;
end;

procedure AddReactionTime(value: integer);
begin
  with Game do
  begin
    ReactionHistory[ReactionIndex] := value;
    ReactionIndex := (ReactionIndex + 1) mod 50;
    if ReactionCount < 50 then Inc(ReactionCount);
  end;
end;

// ----- Звук -----
// В GraphABC нет своей звуковой функции — по опыту форума PascalABC.NET
// самый надёжный способ выдать звук: System.Console.Beep(частота, мс)
// напрямую (это стандартный .NET-метод, доступен без uses). У него есть
// нюанс: вызов блокирует поток на всю длительность звука, поэтому здесь
// нарочно короткие миллисекунды — чтобы не подмешивать паузу в тайминг
// реакции (это тот же класс проблемы, что чинили с первым кругом).
procedure PlayHitSound;
begin
  if Settings.SoundEnabled then
    System.Console.Beep(1000, 30);
end;

procedure PlayMissSound;
begin
  if Settings.SoundEnabled then
    System.Console.Beep(300, 40);
end;

procedure PlayGameOverSound;
begin
  // Экран уже сменился на scGameOver, тайминг реакции тут ни при чём —
  // можно позволить себе более длинный и заметный сигнал
  if Settings.SoundEnabled then
    System.Console.Beep(200, 220);
end;

// ----- Эффекты попадания -----
procedure SpawnEffect(eType: integer; x, y, vx, vy: real; duration: integer; col: Color; txt: string);
begin
  Effects[NextEffectSlot].Active := true;
  Effects[NextEffectSlot].EType := eType;
  Effects[NextEffectSlot].X := x;
  Effects[NextEffectSlot].Y := y;
  Effects[NextEffectSlot].VX := vx;
  Effects[NextEffectSlot].VY := vy;
  Effects[NextEffectSlot].StartTime := Milliseconds;
  Effects[NextEffectSlot].Duration := duration;
  Effects[NextEffectSlot].Col := col;
  Effects[NextEffectSlot].Text := txt;
  // Кольцевой буфер: слотов с запасом (100) на короткоживущие (≤700мс)
  // эффекты, так что даже частые попадания не догонят и не перезапишут
  // ещё не отыгранный эффект.
  NextEffectSlot := (NextEffectSlot + 1) mod MAX_EFFECTS;
end;

procedure SpawnHitEffects(x, y: integer; col: Color; points: integer);
var
  i: integer;
  angle: real;
begin
  SpawnEffect(0, x, y, 0, 0, 250, col, '');       // вспышка
  SpawnEffect(1, x, y, 0, 0, 400, col, '');       // кольцевая волна
  for i := 1 to 8 do                               // осколки веером
  begin
    angle := (i / 8) * 2 * Pi + Random * 0.5;
    SpawnEffect(2, x, y, Cos(angle) * (60 + Random(40)), Sin(angle) * (60 + Random(40)), 500, col, '');
  end;
  SpawnEffect(3, x, y, 0, -30, 700, clWhite, '+' + points.ToString); // всплывающие очки
end;

procedure RenderEffects;
var
  i, r, alpha, ex, ey: integer;
  t: real;
begin
  for i := 0 to MAX_EFFECTS - 1 do
  begin
    if not Effects[i].Active then Continue;
    if Milliseconds - Effects[i].StartTime >= Effects[i].Duration then
    begin
      Effects[i].Active := false;
      Continue;
    end;
    t := (Milliseconds - Effects[i].StartTime) / Effects[i].Duration;

    case Effects[i].EType of
      0: begin // вспышка
        r := round(6 + 34 * t);
        alpha := round(180 * (1 - t));
        Brush.Color := ARGB(alpha, GetRed(Effects[i].Col), GetGreen(Effects[i].Col), GetBlue(Effects[i].Col));
        FillCircle(round(Effects[i].X), round(Effects[i].Y), r);
      end;
      1: begin // кольцевая волна
        r := round(10 + 50 * t);
        alpha := round(160 * (1 - t));
        Pen.Color := ARGB(alpha, 255, 255, 255);
        Pen.Width := 2;
        Circle(round(Effects[i].X), round(Effects[i].Y), r);
      end;
      2: begin // осколок
        ex := round(Effects[i].X + Effects[i].VX * t);
        ey := round(Effects[i].Y + Effects[i].VY * t + 40 * t * t);
        alpha := round(220 * (1 - t));
        Brush.Color := ARGB(alpha, GetRed(Effects[i].Col), GetGreen(Effects[i].Col), GetBlue(Effects[i].Col));
        FillCircle(ex, ey, round(4 * (1 - 0.5 * t)));
      end;
      3: begin // всплывающий текст очков
        ex := round(Effects[i].X);
        ey := round(Effects[i].Y + Effects[i].VY * t);
        alpha := round(255 * (1 - t));
        Font.Size := 16;
        Font.Color := ARGB(alpha, GetRed(Effects[i].Col), GetGreen(Effects[i].Col), GetBlue(Effects[i].Col));
        var tw := TextWidth(Effects[i].Text);
        TextOut(ex - tw div 2, ey, Effects[i].Text);
      end;
      4: begin // экранная вспышка — визуальный акцент на точном попадании
                // в центр вместо реального замедления игрового времени
                // (не хотим трогать FrameTime/TryTime, от точности
                // которых зависит замер реакции)
        alpha := round(70 * (1 - t));
        Brush.Color := ARGB(alpha, 255, 255, 255);
        FillRoundRect(0, 0, W, H, 0, 0);
      end;
      5: begin // красная вспышка на весь экран — подрыв на бомбе
        alpha := round(140 * (1 - t));
        Brush.Color := ARGB(alpha, 200, 30, 20);
        FillRoundRect(0, 0, W, H, 0, 0);
      end;
    end;
  end;
end;

// ----- След курсора (только во время игры — в меню свой эффект частиц) -----
procedure UpdateCursorTrail;
var
  p: Point;
begin
  p := GetMousePos;
  TrailPoints[TrailNext] := p;
  TrailFilled[TrailNext] := true;
  TrailNext := (TrailNext + 1) mod TRAIL_LENGTH;
end;

procedure RenderCursorTrail;
var
  i, idx, r, alpha: integer;
begin
  for i := 0 to TRAIL_LENGTH - 1 do
  begin
    idx := (TrailNext - 1 - i + TRAIL_LENGTH * 2) mod TRAIL_LENGTH;
    if not TrailFilled[idx] then Continue;
    alpha := round(140 * (1 - i / TRAIL_LENGTH));
    r := round(5 * (1 - i / TRAIL_LENGTH)) + 1;
    Brush.Color := ARGB(alpha, 200, 220, 255);
    FillCircle(TrailPoints[idx].X, TrailPoints[idx].Y, r);
  end;
end;

// ----- Темы оформления -----
// Все три темы держим на тёмном фоне: настоящая светлая тема потребовала
// бы переписать цвет текста в каждом Font.Color по всему файлу (их
// десятки), а это неоправданный риск без возможности скомпилировать и
// проверить. "Минимализм" здесь — приглушённая палитра, а не белый фон.
procedure InitThemes;
begin
  Themes[0].Name := 'Тёмная';
  Themes[0].BgColor := RGB(28, 28, 32);
  Themes[0].AccentColor := RGB(97, 207, 255);
  Themes[0].Circle1 := ARGB(220, 97, 207, 255);
  Themes[0].Circle2 := ARGB(220, 255, 80, 80);

  Themes[1].Name := 'Неон';
  Themes[1].BgColor := RGB(12, 8, 24);
  Themes[1].AccentColor := RGB(180, 80, 255);
  Themes[1].Circle1 := ARGB(230, 0, 255, 220);
  Themes[1].Circle2 := ARGB(230, 255, 0, 180);

  Themes[2].Name := 'Минимализм';
  Themes[2].BgColor := RGB(35, 35, 38);
  Themes[2].AccentColor := RGB(160, 160, 165);
  Themes[2].Circle1 := ARGB(220, 150, 150, 160);
  Themes[2].Circle2 := ARGB(220, 180, 140, 140);
end;

procedure ApplyTheme;
begin
  if (CurrentTheme < 0) or (CurrentTheme > 2) then CurrentTheme := 0;
  CircleColors[0] := Themes[CurrentTheme].Circle1;
  CircleColors[1] := Themes[CurrentTheme].Circle2;
end;

function ThemeBg: Color;
begin
  Result := Themes[CurrentTheme].BgColor;
end;

// ----- Достижения -----
procedure InitAchievements;
begin
  AchievementNames[0] := 'Первая игра';
  AchievementDescs[0] := 'Сыграй свою первую игру';
  AchievementNames[1] := 'В яблочко';
  AchievementDescs[1] := 'Попади почти точно в центр цели';
  AchievementNames[2] := 'Молниеносно';
  AchievementDescs[2] := 'Попади с реакцией меньше 150мс';
  AchievementNames[3] := 'Комбо x100';
  AchievementDescs[3] := 'Набери 100 попаданий подряд без промаха';
  AchievementNames[4] := 'Тысяча';
  AchievementDescs[4] := 'Набери 1000 очков за одну игру';
  AchievementNames[5] := 'Снайпер';
  AchievementDescs[5] := 'Закончи игру (от 20 выстрелов) с точностью 90%+';
  AchievementNames[6] := 'Ветеран';
  AchievementDescs[6] := 'Сыграй 1000 игр';
  AchievementNames[7] := 'Мастер Dual';
  AchievementDescs[7] := 'Набери 500+ очков в Dual Mode за одну игру';
end;

procedure InitTutorialTexts;
begin
  TutorialTexts[0] := 'Добро пожаловать в Aim Trainer! Кликай по кругам, пока они не исчезли.';
  TutorialTexts[1] := 'Чем ближе к центру — тем выше Precision (точность попадания).';
  TutorialTexts[2] := 'В Dual Mode синие круги — ЛКМ, красные — ПКМ. С бомбами (красный крест) — осторожно, лучше их не трогать!';
  TutorialTexts[3] := 'R — заново, M — в меню, P — пауза, Esc — закончить попытку. Удачи!';
end;

procedure UnlockAchievement(idx: integer);
begin
  if (idx < 0) or (idx > 7) then Exit;
  if not AchievementUnlocked[idx] then
  begin
    AchievementUnlocked[idx] := true;
    ToastText := 'Достижение: ' + AchievementNames[idx];
    ToastStartTime := Milliseconds;
    ToastActive := true;
    SaveStats; // достижения сохраняются сразу, не дожидаясь конца игры
  end;
end;

procedure RenderToast;
var
  elapsed, alpha, tw: integer;
begin
  if not ToastActive then Exit;
  elapsed := Milliseconds - ToastStartTime;
  if elapsed > 3000 then
  begin
    ToastActive := false;
    Exit;
  end;

  if elapsed < 300 then alpha := round(255 * (elapsed / 300))
  else if elapsed > 2700 then alpha := round(255 * ((3000 - elapsed) / 300))
  else alpha := 255;

  Font.Size := 13;
  tw := TextWidth(ToastText) + 40;
  if tw < 240 then tw := 240;

  Brush.Color := ARGB(round(220 * alpha / 255), 60, 50, 20);
  Pen.Color := ARGB(round(255 * alpha / 255), 255, 210, 90);
  Pen.Width := 2;
  FillRoundRect((W - tw) div 2, 20, (W + tw) div 2, 60, 10, 10);
  Font.Color := ARGB(alpha, 255, 230, 160);
  DrawTextCentered((W - tw) div 2, 20, (W + tw) div 2, 60, ToastText);
end;

// ----- Таблица лидеров -----
procedure TryAddToLeaderboard(mode: TGameMode; score: integer; accuracy: real);
var
  m, i, insertPos, last: integer;
begin
  m := Ord(mode);
  if (m < 0) or (m > 3) then Exit;

  insertPos := LeaderboardCount[m];
  for i := 0 to LeaderboardCount[m] - 1 do
  begin
    if score > Leaderboard[m][i].Score then
    begin
      insertPos := i;
      Break;
    end;
  end;

  if insertPos >= 10 then Exit; // не попал в топ-10

  if LeaderboardCount[m] < 10 then
    last := LeaderboardCount[m]
  else
    last := 9;

  i := last;
  while i > insertPos do
  begin
    Leaderboard[m][i] := Leaderboard[m][i-1];
    Dec(i);
  end;

  Leaderboard[m][insertPos].Score := score;
  Leaderboard[m][insertPos].Accuracy := accuracy;

  if LeaderboardCount[m] < 10 then
    Inc(LeaderboardCount[m]);
end;

// ----- История сессий (CSV, по одному файлу на профиль) -----
// Файл уже в формате CSV с самого начала — "экспорт" делает CopySessionsForExport,
// которая просто складывает читаемую копию под понятным именем рядом.
procedure LogSession(mode: TGameMode; score: integer; acc, avgReaction: real;
                      hits, misses, maxCombo: integer);
var
  f: Text;
  modeStr, dateStr, timeStr: string;
  isNew: boolean;
begin
  try
    // var с выводом типа вместо явного "System.DateTime" в объявлении —
    // полностью квалифицированные ВЫЗОВЫ (как System.Console.Beep раньше)
    // подтверждены, а вот System.DateTime именно как имя ТИПА в var не
    // проверялась — перестраховываемся.
    var now := System.DateTime.Now;
    isNew := not FileExists(SessionsFileName);
    Assign(f, SessionsFileName);
    if isNew then Rewrite(f) else Append(f);
    try
      if isNew then
        Writeln(f, 'Date,Time,Hour,Mode,Score,Accuracy,AvgReaction,Hits,Misses,MaxCombo');

      case mode of
        gmClassic: modeStr := 'Classic';
        gmDual: modeStr := 'Dual';
        gmTracking: modeStr := 'Tracking';
        gmFlick: modeStr := 'Flick';
      end;

      dateStr := now.Year.ToString + '-' + Format('{0:00}', now.Month) + '-' + Format('{0:00}', now.Day);
      timeStr := Format('{0:00}', now.Hour) + ':' + Format('{0:00}', now.Minute);

      Writeln(f, dateStr + ',' + timeStr + ',' + now.Hour.ToString + ',' + modeStr + ',' +
        score.ToString + ',' + Format('{0:0.0}', acc) + ',' + Format('{0:0.0}', avgReaction) + ',' +
        hits.ToString + ',' + misses.ToString + ',' + maxCombo.ToString);
    finally
      Close(f);
    end;
  except
    // Не удалось записать историю сессии — не критично, продолжаем без неё
  end;
end;

// Копия sessions.csv под понятным именем — то же самое содержимое (файл и
// так CSV с первой строки), просто явный, узнаваемый файл для открытия в
// Excel, а не внутренний рабочий файл профиля.
function ExportSessionsCsv: boolean;
var
  fIn, fOut: Text;
  line, outName: string;
begin
  Result := false;
  if not FileExists(SessionsFileName) then Exit;
  try
    var now := System.DateTime.Now;
    outName := 'export_profile' + CurrentProfile.ToString + '_' +
      now.Year.ToString + Format('{0:00}', now.Month) + Format('{0:00}', now.Day) + '.csv';
    Assign(fIn, SessionsFileName);
    Reset(fIn);
    try
      Assign(fOut, outName);
      Rewrite(fOut);
      try
        while not Eof(fIn) do
        begin
          Readln(fIn, line);
          Writeln(fOut, line);
        end;
      finally
        Close(fOut);
      end;
    finally
      Close(fIn);
    end;
    Result := true;
  except
    Result := false;
  end;
end;

function TimeOfDayBucket(hour: integer): integer;
begin
  if (hour >= 6) and (hour < 12) then Result := 0   // Утро
  else if (hour >= 12) and (hour < 18) then Result := 1  // День
  else if (hour >= 18) and (hour < 23) then Result := 2  // Вечер
  else Result := 3;                                       // Ночь
end;

// Перечитывает sessions.csv текущего профиля в память — вызывается только
// когда открывают экран истории, не на каждый кадр
procedure LoadSessionSummary;
var
  f: Text;
  line: string;
  parts: array of string;
  isHeader: boolean;
begin
  LoadedSessionCount := 0;
  if not FileExists(SessionsFileName) then Exit;
  try
    Assign(f, SessionsFileName);
    Reset(f);
    try
      isHeader := true;
      while (not Eof(f)) and (LoadedSessionCount < MAX_SESSIONS_LOADED) do
      begin
        Readln(f, line);
        if isHeader then
        begin
          isHeader := false;
          Continue;
        end;
        parts := line.Split(',');
        if Length(parts) >= 10 then
        begin
          LoadedSessionHour[LoadedSessionCount] := StrToInt(parts[2]);
          LoadedSessionScore[LoadedSessionCount] := StrToInt(parts[4]);
          LoadedSessionAvgReaction[LoadedSessionCount] := StrToFloat(parts[6]);
          Inc(LoadedSessionCount);
        end;
      end;
    finally
      Close(f);
    end;
  except
    LoadedSessionCount := 0;
  end;
end;

// ----- Тепловая карта промахов (текущий забег) -----
procedure RecordMiss(mx, my: integer);
var
  cx, cy: integer;
begin
  cx := (mx * HEATMAP_COLS) div W;
  cy := (my * HEATMAP_ROWS) div H;
  if cx < 0 then cx := 0;
  if cx >= HEATMAP_COLS then cx := HEATMAP_COLS - 1;
  if cy < 0 then cy := 0;
  if cy >= HEATMAP_ROWS then cy := HEATMAP_ROWS - 1;
  MissHeatmap[cx][cy] += 1;
end;

function GetReactionPoints: array of Point;
var
  i, graphW, graphH: integer;
  maxVal: integer;
  history: array[0..49] of integer;
  count: integer;
begin
  if Screen = scGame then
  begin
    count := Game.ReactionCount;
    for i := 0 to 49 do history[i] := Game.ReactionHistory[i];
  end
  else
  begin
    count := SavedReactionCount;
    for i := 0 to 49 do history[i] := SavedReactionHistory[i];
  end;

  if count = 0 then
  begin
    SetLength(Result, 0);
    Exit;
  end;

  maxVal := 1000;
  graphW := W - 100;
  graphH := H div 2 - 110;
  SetLength(Result, count);
  for i := 0 to count - 1 do
  begin
    Result[i].X := 50 + Round(i * graphW / count);
    var val := history[i];
    if val > maxVal then val := maxVal;
    Result[i].Y := (H div 2) + graphH - Round(val * graphH / maxVal);
  end;
end;

// ----- Отрисовка -----
// Рисует плавную кривую через массив точек, используя кривые Безье
procedure DrawSmoothCurve(points: array of Point; color: Color; width: integer);
var
  i, n: integer;
  p0, p1: Point;
  t0x, t0y, t1x, t1y: integer;
  cp1x, cp1y, cp2x, cp2y: integer;
begin
  n := Length(points);
  if n < 2 then Exit;
  if n = 2 then
  begin
    Pen.Color := color;
    Pen.Width := width;
    Line(points[0].X, points[0].Y, points[1].X, points[1].Y);
    Exit;
  end;

  Pen.Color := color;
  Pen.Width := width;

  for i := 0 to n-2 do
  begin
    p0 := points[i];
    p1 := points[i+1];

    // Вычисляем касательные
    if i = 0 then
    begin
      t0x := p1.X - p0.X;
      t0y := p1.Y - p0.Y;
    end
    else
    begin
      t0x := (points[i+1].X - points[i-1].X) div 2;
      t0y := (points[i+1].Y - points[i-1].Y) div 2;
    end;

    if i = n-2 then
    begin
      t1x := p1.X - p0.X;
      t1y := p1.Y - p0.Y;
    end
    else
    begin
      t1x := (points[i+2].X - points[i].X) div 2;
      t1y := (points[i+2].Y - points[i].Y) div 2;
    end;

    // Контрольные точки для кубической кривой
    cp1x := p0.X + t0x div 3;
    cp1y := p0.Y + t0y div 3;
    cp2x := p1.X - t1x div 3;
    cp2y := p1.Y - t1y div 3;

    DrawBezierLine(p0.X, p0.Y, cp1x, cp1y, cp2x, cp2y, p1.X, p1.Y, color, width);
  end;
end;

procedure DrawGraph;
var
  graphX, graphY, graphW, graphH: integer;
  points: array of Point;
  i: integer;
begin
  graphX := 50;
  graphY := H div 2;
  graphW := W - 100;
  graphH := H div 2 - 110;

  Brush.Color := ARGB(150, 30, 30, 35);
  FillRoundRect(graphX, graphY, graphX + graphW, graphY + graphH, 8, 8);

  Font.Size := 10;
  Font.Color := ARGB(200, 200, 200, 200);
  var maxTime := 1000;
  var stepTime := 200;
  for i := 0 to 5 do
  begin
    var val := i * stepTime;
    var yPos := graphY + graphH - Round(val * graphH / maxTime);
    Pen.Color := ARGB(80, 200, 200, 200);
    Pen.Width := 1;
    Line(graphX, yPos, graphX + graphW, yPos);
    DrawTextCentered(graphX - 50, yPos - 8, graphX - 5, yPos + 8, IntToStr(val));
  end;

  // Линия графика (плавная кривая Безье)
  points := GetReactionPoints;
  if Length(points) > 0 then
    DrawSmoothCurve(points, ARGB(255, 97, 207, 255), 2);
end;

procedure RenderGame;
var
  oldFontSize: integer;
  bgColor: Color;
  infoH, colWidth, paddingW, bottom: integer;
  accuracy, avgReaction: real;
  i, livesLeft: integer;
begin
  ClearWindow(ThemeBg);
  RenderCursorTrail;
  for i := 0 to Game.CircleCount - 1 do
    Game.Circles[i].Render(MakeCircleContext, CircleColors);
  RenderEffects;

  oldFontSize := Font.Size;
  Font.Size := 14;
  bgColor := ARGB(220, 60, 60, 65);
  infoH := 60;
  colWidth := W div 5;
  paddingW := 16;
  bottom := 16;

  DrawTextWithBackground(bgColor, 0 + paddingW, H - infoH, colWidth - paddingW, H - bottom, 16,
                         'FPS:'#10 + Game.FPS.ToString, True);
  DrawTextWithBackground(bgColor, colWidth + paddingW, H - infoH, 2*colWidth - paddingW, H - bottom, 16,
                         'Score:'#10 + Game.Score, True);

  // Accuracy = процент попаданий от общего числа выстрелов, считается по
  // Game.Hits (чистое число попаданий), а не по Game.Score — Score теперь
  // очки за размер цели, а не количество попаданий (см. ProcessClick)
  accuracy := IfThen(Game.TotalShots > 0, Game.Hits / Game.TotalShots * 100, 0.0);
  DrawTextWithBackground(bgColor, 2*colWidth + paddingW, H - infoH, 3*colWidth - paddingW, H - bottom, 16,
                         'Accuracy:'#10 + Format('{0:0.0}%', accuracy), True);
  avgReaction := IfThen(Game.Hits > 0, Game.TotalReactionTime / Game.Hits, 0.0);
  DrawTextWithBackground(bgColor, 3*colWidth + paddingW, H - infoH, 4*colWidth - paddingW, H - bottom, 16,
                         'Avg:'#10 + Format('{0:0.0}ms', avgReaction), True);

  livesLeft := Game.MaxLoses - Game.Loses;
  if livesLeft < 0 then livesLeft := 0;
  DrawTextWithBackground(bgColor, 4*colWidth + paddingW, H - infoH, W - paddingW, H - bottom, 16,
                         'Lives:'#10 + livesLeft, True);
  Font.Size := oldFontSize;

  // В Dual Mode — компактная подсказка "какой цвет какой кнопкой".
  // Круги в этом режиме не спавнятся под этой зоной (см. TCircle.Spawn).
  if Game.GameMode = gmDual then
  begin
    oldFontSize := Font.Size;
    Brush.Color := ARGB(160, 40, 40, 45);
    FillRoundRect(10, 10, 190, 42, 8, 8);
    Brush.Color := CircleColors[0];
    FillCircle(26, 26, 7);
    Brush.Color := CircleColors[1];
    FillCircle(106, 26, 7);
    Font.Size := 11;
    Font.Color := clWhite;
    DrawTextCentered(36, 18, 96, 34, 'LMB');
    DrawTextCentered(116, 18, 180, 34, 'RMB');
    Font.Size := oldFontSize;
  end;

  // Индикатор текущей серии — виден только пока она реально что-то значит
  if Game.Combo >= 2 then
  begin
    oldFontSize := Font.Size;
    Brush.Color := ARGB(160, 40, 40, 45);
    FillRoundRect(W - 130, 10, W - 10, 40, 8, 8);
    Font.Size := 14;
    Font.Color := IfThen(Game.Combo >= 10, ARGB(255, 255, 210, 90), clWhite);
    DrawTextCentered(W - 130, 10, W - 10, 40, 'Combo x' + Game.Combo.ToString);
    Font.Size := oldFontSize;
  end;

  RenderToast;
end;

// ----- Плашка предпросмотра режима -----
procedure DrawPlateBg(x, y, w, h, r: integer; bg, border: Color);
begin
  Brush.Color := bg;
  Pen.Color := border;
  FillRoundRect(x, y, x + w, y + h, r, r);
end;

procedure DrawModePlate(x, y, w, h, modeIdx: integer);
var
  ax, ay, r, alpha, bgAlpha, borderAlpha: integer;
  t, phase, pulse: real;
  col: Color;
  i: integer;
  hint: string;
  isHover: boolean;
  mx, my: integer;
begin
  mx := GetMousePos.X;
  my := GetMousePos.Y;
  // Проверяем, наведена ли мышь на эту плашку
  isHover := (mx >= PLATE_X) and (mx <= PLATE_X + PLATE_W) and
             (my >= PLATE_Y) and (my <= PLATE_Y + PLATE_H);;

  // Базовая прозрачность фона и границы
  bgAlpha := 45;
  borderAlpha := 70;

  // Если наведены — пульсация с периодом 1.5 секунды
  if isHover then
  begin
    pulse := 0.5 + 0.5 * sin((MenuAnimTimer mod 1500) / 1500.0 * 2 * Pi);
    bgAlpha := Round(45 + 30 * pulse);      // от 45 до 200
    borderAlpha := Round(70 + 150 * pulse);  // от 70 до 220
  end;

  // Рисуем фон плашки с рассчитанной прозрачностью
  DrawPlateBg(x, y, w, h, 12,
    ARGB(bgAlpha, 255, 255, 255),
    ARGB(borderAlpha, 255, 255, 255));

  Font.Size := 18;
  Font.Color := clWhite;
  Brush.Color := ARGB(0, 0, 0, 0);
  SetFontStyle(fsBold);

  ax := x + w div 2;
  ay := y + h div 2 + 12;
  t := (MenuAnimTimer mod 2000) / 2000;

  case Modes[modeIdx].AnimType of
    0: begin
      var tw := TextWidth('Режим - Classic');
      TextOut(x + (w - tw) div 2, y + 14, 'Режим - Classic');
      SetFontStyle(fsNormal);
      r := round(48 * (1 - t));
      if r < 4 then r := 4;
      alpha := 255;
      if t > 0.85 then alpha := round(255 * (1 - (t - 0.85) / 0.15));
      Brush.Color := ARGB(alpha, GetRed(Modes[modeIdx].CircleColor), GetGreen(Modes[modeIdx].CircleColor), GetBlue(Modes[modeIdx].CircleColor));
      FillCircle(ax, ay, r);
      Pen.Color := ARGB(alpha, 180, 220, 255);
      Pen.Width := 2;
      Circle(ax, ay, r);
      Pen.Color := ARGB(alpha, 255, 255, 255);
      Pen.Width := 1;
      Line(ax - 6, ay, ax + 6, ay);
      Line(ax, ay - 6, ax, ay + 6);
    end;
    1: begin
      var tw := TextWidth('Режим - Dual');
      TextOut(x + (w - tw) div 2, y + 14, 'Режим - Dual');
      SetFontStyle(fsNormal);
      phase := (MenuAnimTimer mod 4000) / 2000;
      var idx := trunc(phase) mod 2;
      col := IfThen(idx = 0, Modes[modeIdx].CircleColor, Modes[modeIdx].CircleColor2);
      var t2 := frac(phase);
      r := round(42 * (1 - t2));
      if r < 4 then r := 4;
      alpha := 255;
      if t2 > 0.85 then alpha := round(255 * (1 - (t2 - 0.85) / 0.15));
      Brush.Color := ARGB(alpha, GetRed(col), GetGreen(col), GetBlue(col));
      FillCircle(ax, ay, r);
      Pen.Color := ARGB(alpha, 255, 255, 255);
      Pen.Width := 2;
      Circle(ax, ay, r);
      Pen.Color := ARGB(alpha, 255, 255, 255);
      Pen.Width := 1;
      Line(ax - 6, ay, ax + 6, ay);
      Line(ax, ay - 6, ax, ay + 6);
    end;
    2: begin
      var tw := TextWidth('Режим - Tracking');
      TextOut(x + (w - tw) div 2, y + 14, 'Режим - Tracking');
      SetFontStyle(fsNormal);
      var offset := round(55 * sin(MenuAnimTimer / 700));
      r := 20;
      for i := 1 to 6 do
      begin
        var trailA := round(60 - i * 10);
        var trailOff := round(55 * sin((MenuAnimTimer - i * 40) / 700));
        Brush.Color := ARGB(trailA, GetRed(Modes[modeIdx].CircleColor), GetGreen(Modes[modeIdx].CircleColor), GetBlue(Modes[modeIdx].CircleColor));
        FillCircle(ax + trailOff, ay, r - i * 2);
      end;
      Brush.Color := ARGB(220, GetRed(Modes[modeIdx].CircleColor), GetGreen(Modes[modeIdx].CircleColor), GetBlue(Modes[modeIdx].CircleColor));
      FillCircle(ax + offset, ay, r);
      Pen.Color := clWhite;
      Pen.Width := 2;
      Circle(ax + offset, ay, r);
      Pen.Color := RGB(255, 255, 255);
      Pen.Width := 1;
      Line(ax - 6 + offset, ay, ax + 6 + offset, ay);
      Line(ax + offset, ay - 6, ax + offset, ay + 6);
    end;
    3: begin
      var tw := TextWidth('Режим - Flick');
      TextOut(x + (w - tw) div 2, y + 14, 'Режим - Flick');
      SetFontStyle(fsNormal);
      var flickT := (MenuAnimTimer mod 500) / 500;
      if flickT < 0.25 then
      begin
        r := round(45 * (1 - flickT / 0.25));
        alpha := round(255 * (1 - flickT / 0.25));
        Brush.Color := ARGB(alpha, GetRed(Modes[modeIdx].CircleColor), GetGreen(Modes[modeIdx].CircleColor), GetBlue(Modes[modeIdx].CircleColor));
        FillCircle(ax, ay, r);
        Pen.Color := ARGB(alpha, 255, 255, 255);
        Pen.Width := 2;
        Circle(ax, ay, r);
        Pen.Color := ARGB(alpha, 255, 255, 255);
        Pen.Width := 1;
        Line(ax - 6, ay, ax + 6, ay);
        Line(ax, ay - 6, ax, ay + 6);
      end;
    end;
  end;

  Font.Size := 10;
  Font.Color := RGB(255, 255, 255);
  Brush.Color := ARGB(0, 0, 0, 0);
  hint := IfThen(Modes[modeIdx].Implemented, 'Кликни для старта', 'Режим в разработке');
  var hw := TextWidth(hint);
  TextOut(x + (w - hw) div 2, y + h - 24, hint);
end;

procedure DrawModeButtons;
var
  i, bx, by, entryAlpha, entryOffsetY, baseAlpha, pulseAlpha, elapsedSinceEnter: integer;
  col: Color;
  isSel: boolean;
  localT, pulsePhase, pulse: real;
begin
  elapsedSinceEnter := Milliseconds - MenuEnterTick;
  if elapsedSinceEnter < 0 then elapsedSinceEnter := 999999;

  for i := 0 to ModeCount - 1 do
  begin
    // Плавное появление кнопок каскадом: i-я кнопка начинает появляться
    // через i*80мс после входа в меню, само появление занимает 400мс —
    // фейд по альфе плюс небольшой сдвиг снизу вверх.
    localT := (elapsedSinceEnter - i * 80) / 400.0;
    if localT < 0 then localT := 0;
    if localT > 1 then localT := 1;
    entryAlpha := round(255 * localT);
    entryOffsetY := round((1 - localT) * 20);

    bx := BTNS_X;
    by := BTNS_Y + i * (BTNS_H + BTNS_GAP) + entryOffsetY;
    isSel := (SelectedModeIndex = i);

    if not Modes[i].Implemented then
      baseAlpha := 70
    else if isSel then
      baseAlpha := 255
    else
      baseAlpha := 120;
    baseAlpha := (baseAlpha * entryAlpha) div 255;

    col := ARGB(baseAlpha, GetRed(Modes[i].ButtonColor), GetGreen(Modes[i].ButtonColor), GetBlue(Modes[i].ButtonColor));

    Brush.Color := col;
    Pen.Color := ARGB((80 * entryAlpha) div 255, 255, 255, 255);
    Pen.Width := 1;
    FillRoundRect(bx, by, bx + BTNS_W, by + BTNS_H, 8, 8);

    if isSel then
    begin
      Pen.Color := ARGB(entryAlpha, 255, 255, 255);
      Pen.Width := 2;
      FillRoundRect(bx, by, bx + BTNS_W, by + BTNS_H, 8, 8);
      Pen.Width := 1;
    end;

    Font.Size := 13;
    Font.Color := ARGB(entryAlpha, 255, 255, 255);
    Brush.Color := ARGB(0, 0, 0, 0);
    SetFontStyle(fsBold);
    var tw := TextWidth(Modes[i].Name);
    TextOut(bx + (BTNS_W - tw) div 2, by + (BTNS_H - 16) div 2, Modes[i].Name);
    SetFontStyle(fsNormal);

    if not Modes[i].Implemented then
    begin
      Font.Size := 9;
      Font.Color := ARGB((210 * entryAlpha) div 255, 255, 210, 120);
      TextOut(bx + 6, by + 3, 'СКОРО');
    end;
  end;
end;

procedure DrawModeDescription;
var
  desc: string;
  startPos, endPos, spacePos, lineCount, ly, MAX_CHARS: integer;
begin
  DrawPlateBg(DESC_X, DESC_Y, DESC_W, DESC_H, 8,
    ARGB(35, 255, 255, 255),
    ARGB(55, 255, 255, 255));

  Font.Size := 11;
  Font.Color := ARGB(220, 255, 255, 255);

  MAX_CHARS := Round(330 / TextWidth('W'));
  desc := Modes[SelectedModeIndex].Description;
  startPos := 1;
  ly := DESC_Y + 14;
  lineCount := 0;

  while (startPos <= Length(desc)) and (lineCount < 4) do
  begin
    endPos := startPos + MAX_CHARS - 1;
    if endPos > Length(desc) then endPos := Length(desc);

    spacePos := endPos;
    while (spacePos > startPos) and (desc[spacePos] <> ' ') do
      Dec(spacePos);
    if spacePos <= startPos then spacePos := endPos;

    var line := Copy(desc, startPos, spacePos - startPos + 1);
    Brush.Color := ARGB(0, 0, 0, 0);
    TextOut(DESC_X + 12, ly, line);
    ly += 18;
    startPos := spacePos + 1;
    Inc(lineCount);
  end;
end;

function GetModeBtnAt(mx, my, oldV: integer): integer;
var i, bx, by: integer;
begin
  Result := oldV;
  for i := 0 to ModeCount - 1 do
  begin
    bx := BTNS_X;
    by := BTNS_Y + i * (BTNS_H + BTNS_GAP);
    if (mx >= bx) and (mx <= bx + BTNS_W) and (my >= by) and (my <= by + BTNS_H) then
    begin
      Result := i;
      Exit;
    end;
  end;
end;

function GetModeBtnPressed(mx, my: integer): integer;
var i, bx, by: integer;
begin
  Result := -1;
  for i := 0 to ModeCount - 1 do
  begin
    bx := BTNS_X;
    by := BTNS_Y + i * (BTNS_H + BTNS_GAP);
    if (mx >= bx) and (mx <= bx + BTNS_W) and (my >= by) and (my <= by + BTNS_H) then
    begin
      Result := i;
      Exit;
    end;
  end;
end;

function IsInPlate(mx, my: integer): boolean;
begin
  Result := (mx >= PLATE_X) and (mx <= PLATE_X + PLATE_W) and
            (my >= PLATE_Y) and (my <= PLATE_Y + PLATE_H);
end;

function IsInHelpButton(mx, my: integer): boolean;
begin
  Result := (mx >= 10) and (mx <= 105) and (my >= H - 40) and (my <= H - 5);
end;

function IsInSettingsButton(mx, my: integer): boolean;
begin
  Result := (mx >= 105) and (mx <= 205) and (my >= H - 40) and (my <= H - 5);
end;

function IsInLeaderboardButton(mx, my: integer): boolean;
begin
  Result := (mx >= 205) and (mx <= 305) and (my >= H - 40) and (my <= H - 5);
end;

function IsInAchievementsButton(mx, my: integer): boolean;
begin
  Result := (mx >= 305) and (mx <= 405) and (my >= H - 40) and (my <= H - 5);
end;

function IsInHistoryButton(mx, my: integer): boolean;
begin
  Result := (mx >= 405) and (mx <= 505) and (my >= H - 40) and (my <= H - 5);
end;

function IsInProfileButton(mx, my: integer): boolean;
begin
  Result := (mx >= 505) and (mx <= 605) and (my >= H - 40) and (my <= H - 5);
end;

// ----- Панель настроек (размер круга / время жизни цели / жизни / звук) -----
function SettingsPanelX: integer;
begin
  Result := (W - SET_PANEL_W) div 2;
end;

function SettingsPanelY: integer;
begin
  Result := (H - SET_PANEL_H) div 2;
end;

// row 0=размер круга, 1=время жизни, 2=жизни, 3=звук, 4=кнопка сброса
function SettingsRowY(row: integer): integer;
begin
  Result := SettingsPanelY + 80 + row * SET_ROW_H;
end;

function IsInSettingsPanel(mx, my: integer): boolean;
begin
  Result := (mx >= SettingsPanelX) and (mx <= SettingsPanelX + SET_PANEL_W) and
            (my >= SettingsPanelY) and (my <= SettingsPanelY + SET_PANEL_H);
end;

function IsInSettingsMinus(row, mx, my: integer): boolean;
var px, py: integer;
begin
  px := SettingsPanelX + SET_PANEL_W - 2*SET_BTN_W - 56;
  py := SettingsRowY(row);
  Result := (mx >= px) and (mx <= px + SET_BTN_W) and (my >= py) and (my <= py + SET_BTN_W);
end;

function IsInSettingsPlus(row, mx, my: integer): boolean;
var px, py: integer;
begin
  px := SettingsPanelX + SET_PANEL_W - SET_BTN_W - 16;
  py := SettingsRowY(row);
  Result := (mx >= px) and (mx <= px + SET_BTN_W) and (my >= py) and (my <= py + SET_BTN_W);
end;

function IsInSoundToggle(mx, my: integer): boolean;
var py: integer;
begin
  py := SettingsRowY(3);
  Result := (mx >= SettingsPanelX + 20) and (mx <= SettingsPanelX + SET_PANEL_W - 20) and
            (my >= py) and (my <= py + SET_BTN_W);
end;

function IsInThemeToggle(mx, my: integer): boolean;
var py: integer;
begin
  py := SettingsRowY(4);
  Result := (mx >= SettingsPanelX + 20) and (mx <= SettingsPanelX + SET_PANEL_W - 20) and
            (my >= py) and (my <= py + SET_BTN_W);
end;

function IsInSettingsReset(mx, my: integer): boolean;
var px, py: integer;
begin
  px := SettingsPanelX + (SET_PANEL_W - 180) div 2;
  py := SettingsRowY(5) + 6;
  Result := (mx >= px) and (mx <= px + 180) and (my >= py) and (my <= py + 32);
end;

procedure StepSetting(row, delta: integer);
begin
  case row of
    0: begin
      Settings.CircleSize += delta * 2;
      if Settings.CircleSize < MIN_CIRCLE_SIZE then Settings.CircleSize := MIN_CIRCLE_SIZE;
      if Settings.CircleSize > MAX_CIRCLE_SIZE then Settings.CircleSize := MAX_CIRCLE_SIZE;
    end;
    1: begin
      Settings.LifetimeMs += delta * LIFETIME_STEP;
      if Settings.LifetimeMs < MIN_LIFETIME then Settings.LifetimeMs := MIN_LIFETIME;
      if Settings.LifetimeMs > MAX_LIFETIME then Settings.LifetimeMs := MAX_LIFETIME;
    end;
    2: begin
      Settings.Lives += delta;
      if Settings.Lives < MIN_LIVES then Settings.Lives := MIN_LIVES;
      if Settings.Lives > MAX_LIVES then Settings.Lives := MAX_LIVES;
    end;
  end;
  SaveStats; // настройки применятся со следующей игры, но сохраняем сразу
end;

// ----- История сессий: геометрия панели -----
function HistoryPanelX: integer;
begin
  Result := (W - 520) div 2;
end;

function HistoryPanelY: integer;
begin
  Result := (H - 460) div 2;
end;

function IsInHistoryPanel(mx, my: integer): boolean;
begin
  Result := (mx >= HistoryPanelX) and (mx <= HistoryPanelX + 520) and
            (my >= HistoryPanelY) and (my <= HistoryPanelY + 460);
end;

function IsInExportButton(mx, my: integer): boolean;
var px, py: integer;
begin
  px := HistoryPanelX + 20;
  py := HistoryPanelY + 410;
  Result := (mx >= px) and (mx <= px + 220) and (my >= py) and (my <= py + 32);
end;

// ----- Профили: геометрия панели -----
function ProfilesPanelX: integer;
begin
  Result := (W - 360) div 2;
end;

function ProfilesPanelY: integer;
begin
  Result := (H - 320) div 2;
end;

function IsInProfilesPanel(mx, my: integer): boolean;
begin
  Result := (mx >= ProfilesPanelX) and (mx <= ProfilesPanelX + 360) and
            (my >= ProfilesPanelY) and (my <= ProfilesPanelY + 320);
end;

function GetProfileRowAt(mx, my: integer): integer;
var i, py: integer;
begin
  Result := -1;
  for i := 1 to MAX_PROFILES do
  begin
    py := ProfilesPanelY + 60 + (i - 1) * 50;
    if (mx >= ProfilesPanelX + 20) and (mx <= ProfilesPanelX + 340) and
       (my >= py) and (my <= py + 40) then
    begin
      Result := i;
      Exit;
    end;
  end;
end;

// Переключение профиля — это просто повторный LoadStats на новое имя
// файла: Stats/Settings/Leaderboard/Achievements/ReactionHistory уже
// полностью сбрасываются и загружаются заново внутри LoadStats.
procedure SwitchProfile(newProfile: integer);
begin
  if (newProfile < 1) or (newProfile > MAX_PROFILES) then Exit;
  if newProfile = CurrentProfile then Exit;
  CurrentProfile := newProfile;
  LoadStats;
  ApplyTheme;
end;

procedure UpdateMenuInput;
var
  mx, my, row: integer;
  mp: Point;
begin
  mp := GetMousePos;
  mx := mp.X;
  my := mp.Y;

  if not ShowHelp and not ShowSettings and not ShowLeaderboard
     and not ShowAchievements and not ShowTutorial
     and not ShowHistory and not ShowProfiles then
    SelectedModeIndex := GetModeBtnAt(mx, my, SelectedModeIndex);

  while IsMouseButtonPressed(1) do
  begin
    // Туториал первого запуска — приоритет выше всего остального. Клик
    // просто листает шаги; на последнем — закрывает и больше не покажется.
    if ShowTutorial then
    begin
      Inc(TutorialStep);
      if TutorialStep >= 4 then
      begin
        ShowTutorial := false;
        Stats.TutorialSeen := true;
        SaveStats;
      end;
      ClearMouseButtonPressed(1);
      Exit;
    end;

    // Пока открыта справка — любой клик её закрывает и не идёт дальше
    // в обычную обработку меню (не хотим одним и тем же кликом ещё и
    // выбрать режим под панелью).
    if ShowHelp then
    begin
      ShowHelp := false;
      ClearMouseButtonPressed(1);
      Exit;
    end;

    if ShowLeaderboard then
    begin
      ShowLeaderboard := false;
      ClearMouseButtonPressed(1);
      Exit;
    end;

    if ShowAchievements then
    begin
      ShowAchievements := false;
      ClearMouseButtonPressed(1);
      Exit;
    end;

    if ShowHistory then
    begin
      if IsInExportButton(mx, my) then
      begin
        if ExportSessionsCsv then
        begin
          ToastText := 'CSV экспортирован успешно';
          ToastStartTime := Milliseconds;
          ToastActive := true;
        end
        else
        begin
          ToastText := 'Ошибка: нет данных для экспорта';
          ToastStartTime := Milliseconds;
          ToastActive := true;
        end;
      end
      else if not IsInHistoryPanel(mx, my) then
        ShowHistory := false;
      ClearMouseButtonPressed(1);
      Exit;
    end;

    if ShowProfiles then
    begin
      var pr := GetProfileRowAt(mx, my);
      if pr >= 1 then
        SwitchProfile(pr)
      else if not IsInProfilesPanel(mx, my) then
        ShowProfiles := false;
      ClearMouseButtonPressed(1);
      Exit;
    end;

    // Настройки: клики внутри панели что-то меняют, клик снаружи закрывает
    if ShowSettings then
    begin
      for row := 0 to 2 do
      begin
        if IsInSettingsMinus(row, mx, my) then StepSetting(row, -1)
        else if IsInSettingsPlus(row, mx, my) then StepSetting(row, 1);
      end;
      if IsInSoundToggle(mx, my) then
      begin
        Settings.SoundEnabled := not Settings.SoundEnabled;
        SaveStats;
      end
      else if IsInThemeToggle(mx, my) then
      begin
        CurrentTheme := (CurrentTheme + 1) mod 3;
        ApplyTheme;
        SaveStats;
      end
      else if IsInSettingsReset(mx, my) then
      begin
        SetDefaultSettings;
        SaveStats;
      end
      else if not IsInSettingsPanel(mx, my) then
        ShowSettings := false;
      ClearMouseButtonPressed(1);
      Exit;
    end;

    if IsInHelpButton(mx, my) then
    begin
      ShowHelp := true;
      ClearMouseButtonPressed(1);
      Exit;
    end;

    if IsInSettingsButton(mx, my) then
    begin
      ShowSettings := true;
      ClearMouseButtonPressed(1);
      Exit;
    end;

    if IsInLeaderboardButton(mx, my) then
    begin
      ShowLeaderboard := true;
      ClearMouseButtonPressed(1);
      Exit;
    end;

    if IsInAchievementsButton(mx, my) then
    begin
      ShowAchievements := true;
      ClearMouseButtonPressed(1);
      Exit;
    end;

    if IsInHistoryButton(mx, my) then
    begin
      ShowHistory := true;
      LoadSessionSummary;
      ClearMouseButtonPressed(1);
      Exit;
    end;

    if IsInProfileButton(mx, my) then
    begin
      ShowProfiles := true;
      ClearMouseButtonPressed(1);
      Exit;
    end;
    
    if (GetModeBtnPressed(mx, my) <> -1) and Modes[SelectedModeIndex].Implemented then
    begin
      Game.GameMode := Modes[SelectedModeIndex].GameMode;
      Screen := scCountdown;
      Exit;
    end;

    if IsInPlate(mx, my) then
    begin
      // Запуск блокирован для режимов-заготовок (Implemented = false) —
      // у них пока нет реальной игровой механики, см. UpdateGame/TCircle
      if Modes[SelectedModeIndex].Implemented then
      begin
        Game.GameMode := Modes[SelectedModeIndex].GameMode;
        Screen := scCountdown;
        Exit;
      end;
    end;
    
    ShowMainMenuEffects := not ShowMainMenuEffects;
    ClearMouseButtonPressed(1);
  end;
end;

procedure RenderHelpOverlay;
var
  panelX, panelY, panelW, panelH, ly: integer;
begin
  Brush.Color := ARGB(210, 15, 15, 18);
  FillRoundRect(0, 0, W, H, 1, 1);

  panelW := 460; panelH := 330;
  panelX := (W - panelW) div 2;
  panelY := (H - panelH) div 2;

  Brush.Color := ARGB(235, 45, 45, 50);
  Pen.Color := ARGB(255, 90, 90, 95);
  Pen.Width := 1;
  FillRoundRect(panelX, panelY, panelX + panelW, panelY + panelH, 14, 14);

  Font.Size := 20;
  Font.Color := clWhite;
  DrawTextCentered(panelX, panelY + 16, panelX + panelW, panelY + 46, 'Справка');

  Font.Size := 13;
  Font.Color := ARGB(230, 220, 220, 220);
  ly := panelY + 64;
  DrawTextCentered(panelX + 20, ly, panelX + panelW - 20, ly + 20, 'ЛКМ — выстрел / "синяя" кнопка в Dual Mode'); ly += 24;
  DrawTextCentered(panelX + 20, ly, panelX + panelW - 20, ly + 20, 'ПКМ — "красная" кнопка в Dual Mode'); ly += 24;
  DrawTextCentered(panelX + 20, ly, panelX + panelW - 20, ly + 20, 'R — начать текущий режим заново'); ly += 24;
  DrawTextCentered(panelX + 20, ly, panelX + panelW - 20, ly + 20, 'M — выйти в меню'); ly += 24;
  DrawTextCentered(panelX + 20, ly, panelX + panelW - 20, ly + 20, 'P — пауза'); ly += 24;
  DrawTextCentered(panelX + 20, ly, panelX + panelW - 20, ly + 20, 'Esc — закончить попытку'); ly += 34;
  DrawTextCentered(panelX + 20, ly, panelX + panelW - 20, ly + 20, 'Accuracy — % попаданий от всех выстрелов'); ly += 24;
  DrawTextCentered(panelX + 20, ly, panelX + panelW - 20, ly + 20, 'Precision — точность по центру круга'); ly += 34;
  DrawTextCentered(panelX + 20, ly, panelX + panelW - 20, ly + 20, 'Score — очки: меньше круг — больше очков'); ly += 24;

  Font.Size := 11;
  Font.Color := ARGB(180, 180, 180, 180);
  DrawTextCentered(panelX, panelY + panelH - 26, panelX + panelW, panelY + panelH - 6, 'Клик — закрыть');
end;

procedure DrawSettingsStepper(row: integer);
var
  py, mx1, px1: integer;
begin
  py := SettingsRowY(row);
  mx1 := SettingsPanelX + SET_PANEL_W - 2*SET_BTN_W - 56;
  px1 := SettingsPanelX + SET_PANEL_W - SET_BTN_W - 16;

  Brush.Color := ARGB(150, 255, 255, 255);
  Pen.Color := ARGB(0, 0, 0, 0);
  FillRoundRect(mx1, py, mx1 + SET_BTN_W, py + SET_BTN_W, 6, 6);
  Font.Size := 16;
  Font.Color := ARGB(255, 30, 30, 34);
  DrawTextCentered(mx1, py, mx1 + SET_BTN_W, py + SET_BTN_W, '-');

  Brush.Color := ARGB(150, 255, 255, 255);
  FillRoundRect(px1, py, px1 + SET_BTN_W, py + SET_BTN_W, 6, 6);
  DrawTextCentered(px1, py, px1 + SET_BTN_W, py + SET_BTN_W, '+');
end;

procedure RenderSettingsOverlay;
var
  panelX, panelY, py: integer;
begin
  Brush.Color := ARGB(210, 15, 15, 18);
  FillRoundRect(0, 0, W, H, 1, 1);

  panelX := SettingsPanelX;
  panelY := SettingsPanelY;

  Brush.Color := ARGB(235, 45, 45, 50);
  Pen.Color := ARGB(255, 90, 90, 95);
  Pen.Width := 1;
  FillRoundRect(panelX, panelY, panelX + SET_PANEL_W, panelY + SET_PANEL_H, 14, 14);

  Font.Size := 20;
  Font.Color := clWhite;
  DrawTextCentered(panelX, panelY + 14, panelX + SET_PANEL_W, panelY + 44, 'Настройки');

  Font.Size := 13;
  Font.Color := ARGB(230, 220, 220, 220);

  py := SettingsRowY(0);
  DrawTextCentered(panelX + 20, py, panelX + SET_PANEL_W - 130, py + SET_BTN_W, 'Размер круга: ' + Settings.CircleSize.ToString);
  DrawSettingsStepper(0);

  py := SettingsRowY(1);
  Font.Color := ARGB(230, 220, 220, 220);
  DrawTextCentered(panelX + 20, py, panelX + SET_PANEL_W - 130, py + SET_BTN_W, 'Время жизни: ' + Settings.LifetimeMs.ToString + ' мс');
  DrawSettingsStepper(1);

  py := SettingsRowY(2);
  Font.Color := ARGB(230, 220, 220, 220);
  DrawTextCentered(panelX + 20, py, panelX + SET_PANEL_W - 130, py + SET_BTN_W, 'Жизни: ' + Settings.Lives.ToString);
  DrawSettingsStepper(2);

  py := SettingsRowY(3);
  Brush.Color := ARGB(60, 255, 255, 255);
  FillRoundRect(panelX + 20, py, panelX + SET_PANEL_W - 20, py + SET_BTN_W, 8, 8);
  Font.Color := clWhite;
  DrawTextCentered(panelX + 20, py, panelX + SET_PANEL_W - 20, py + SET_BTN_W,
    'Звук: ' + IfThen(Settings.SoundEnabled, 'ON', 'OFF'));

  py := SettingsRowY(4);
  Brush.Color := ARGB(60, 255, 255, 255);
  FillRoundRect(panelX + 20, py, panelX + SET_PANEL_W - 20, py + SET_BTN_W, 8, 8);
  Font.Color := clWhite;
  DrawTextCentered(panelX + 20, py, panelX + SET_PANEL_W - 20, py + SET_BTN_W,
    'Тема: ' + Themes[CurrentTheme].Name);

  py := SettingsRowY(5) + 6;
  Brush.Color := ARGB(160, 90, 90, 95);
  FillRoundRect(panelX + (SET_PANEL_W - 180) div 2, py, panelX + (SET_PANEL_W - 180) div 2 + 180, py + 32, 8, 8);
  Font.Size := 12;
  Font.Color := clWhite;
  DrawTextCentered(panelX + (SET_PANEL_W - 180) div 2, py, panelX + (SET_PANEL_W - 180) div 2 + 180, py + 32, 'Сбросить по умолчанию');

  Font.Size := 11;
  Font.Color := ARGB(180, 180, 180, 180);
  DrawTextCentered(panelX, panelY + SET_PANEL_H - 24, panelX + SET_PANEL_W, panelY + SET_PANEL_H - 6,
    'Клик вне панели — закрыть. Изменения — со следующей игры.');
end;

procedure RenderLeaderboardOverlay;
var
  panelX, panelY, panelW, panelH, i, ly: integer;
begin
  Brush.Color := ARGB(210, 15, 15, 18);
  FillRoundRect(0, 0, W, H, 1, 1);

  panelW := 480; panelH := 420;
  panelX := (W - panelW) div 2;
  panelY := (H - panelH) div 2;

  Brush.Color := ARGB(235, 45, 45, 50);
  Pen.Color := ARGB(255, 90, 90, 95);
  Pen.Width := 1;
  FillRoundRect(panelX, panelY, panelX + panelW, panelY + panelH, 14, 14);

  Font.Size := 20;
  Font.Color := clWhite;
  DrawTextCentered(panelX, panelY + 14, panelX + panelW, panelY + 44, 'Таблица лидеров');

  Font.Size := 14;
  DrawTextCentered(panelX + 10, panelY + 56, panelX + panelW div 2, panelY + 80, 'Classic Mode');
  DrawTextCentered(panelX + panelW div 2, panelY + 56, panelX + panelW - 10, panelY + 80, 'Dual Mode');

  Font.Size := 12;
  Font.Color := ARGB(220, 220, 220, 220);
  for i := 0 to 9 do
  begin
    ly := panelY + 90 + i * 28;
    if i < LeaderboardCount[0] then
      DrawTextCentered(panelX + 10, ly, panelX + panelW div 2, ly + 24,
        (i+1).ToString + '. ' + Leaderboard[0][i].Score.ToString + ' (' + Format('{0:0.0}%', Leaderboard[0][i].Accuracy) + ')')
    else
      DrawTextCentered(panelX + 10, ly, panelX + panelW div 2, ly + 24, (i+1).ToString + '. —');

    if i < LeaderboardCount[1] then
      DrawTextCentered(panelX + panelW div 2, ly, panelX + panelW - 10, ly + 24,
        (i+1).ToString + '. ' + Leaderboard[1][i].Score.ToString + ' (' + Format('{0:0.0}%', Leaderboard[1][i].Accuracy) + ')')
    else
      DrawTextCentered(panelX + panelW div 2, ly, panelX + panelW - 10, ly + 24, (i+1).ToString + '. —');
  end;

  Font.Size := 11;
  Font.Color := ARGB(180, 180, 180, 180);
  DrawTextCentered(panelX, panelY + panelH - 24, panelX + panelW, panelY + panelH - 6, 'Клик — закрыть');
end;

procedure RenderAchievementsOverlay;
var
  panelX, panelY, panelW, panelH, i, ly: integer;
  col: Color;
begin
  Brush.Color := ARGB(210, 15, 15, 18);
  FillRoundRect(0, 0, W, H, 1, 1);

  panelW := 460; panelH := 440;
  panelX := (W - panelW) div 2;
  panelY := (H - panelH) div 2;

  Brush.Color := ARGB(235, 45, 45, 50);
  Pen.Color := ARGB(255, 90, 90, 95);
  Pen.Width := 1;
  FillRoundRect(panelX, panelY, panelX + panelW, panelY + panelH, 14, 14);

  Font.Size := 20;
  Font.Color := clWhite;
  DrawTextCentered(panelX, panelY + 14, panelX + panelW, panelY + 44, 'Достижения');

  for i := 0 to 7 do
  begin
    ly := panelY + 56 + i * 42;
    if AchievementUnlocked[i] then
      col := ARGB(255, 255, 210, 90)
    else
      col := ARGB(140, 150, 150, 150);
    Brush.Color := ARGB(40, 255, 255, 255);
    FillRoundRect(panelX + 16, ly, panelX + panelW - 16, ly + 36, 8, 8);
    Font.Color := col;
    Font.Size := 13;
    DrawTextCentered(panelX + 26, ly + 2, panelX + panelW - 26, ly + 18, AchievementNames[i]);
    Font.Size := 10;
    Font.Color := ARGB(180, 200, 200, 200);
    DrawTextCentered(panelX + 26, ly + 19, panelX + panelW - 26, ly + 34, AchievementDescs[i]);
  end;

  Font.Size := 11;
  Font.Color := ARGB(180, 180, 180, 180);
  DrawTextCentered(panelX, panelY + panelH - 24, panelX + panelW, panelY + panelH - 6, 'Клик — закрыть');
end;

procedure RenderHistoryOverlay;
var
  panelX, panelY, i, bucket, ly: integer;
  bucketSum: array[0..3] of integer;
  bucketCount: array[0..3] of integer;
  bucketNames: array[0..3] of string;
  points: array of Point;
  graphX, graphY, graphW, graphH, startIdx, cnt, val, maxVal: integer;
  avgTxt: string;
begin
  Brush.Color := ARGB(210, 15, 15, 18);
  FillRoundRect(0, 0, W, H, 1, 1);

  panelX := HistoryPanelX;
  panelY := HistoryPanelY;

  Brush.Color := ARGB(235, 45, 45, 50);
  Pen.Color := ARGB(255, 90, 90, 95);
  Pen.Width := 1;
  FillRoundRect(panelX, panelY, panelX + 520, panelY + 460, 14, 14);

  Font.Size := 20;
  Font.Color := clWhite;
  DrawTextCentered(panelX, panelY + 14, panelX + 520, panelY + 44, 'История');

  Font.Size := 12;
  Font.Color := ARGB(210, 210, 210, 210);
  DrawTextCentered(panelX, panelY + 46, panelX + 520, panelY + 64,
    'Забегов в истории: ' + LoadedSessionCount.ToString);

  // Разбивка по времени суток
  bucketNames[0] := 'Утро (6-12)';
  bucketNames[1] := 'День (12-18)';
  bucketNames[2] := 'Вечер (18-23)';
  bucketNames[3] := 'Ночь (23-6)';
  for i := 0 to 3 do
  begin
    bucketSum[i] := 0;
    bucketCount[i] := 0;
  end;
  for i := 0 to LoadedSessionCount - 1 do
  begin
    bucket := TimeOfDayBucket(LoadedSessionHour[i]);
    bucketSum[bucket] += LoadedSessionScore[i];
    Inc(bucketCount[bucket]);
  end;

  Font.Size := 13;
  ly := panelY + 76;
  for i := 0 to 3 do
  begin
    Font.Color := ARGB(220, 220, 220, 220);
    if bucketCount[i] > 0 then
      avgTxt := bucketNames[i] + ': средний счёт ' + Format('{0:0.0}', bucketSum[i] / bucketCount[i]) +
                ' (' + bucketCount[i].ToString + ' игр)'
    else
      avgTxt := bucketNames[i] + ': нет данных';
    DrawTextCentered(panelX + 20, ly, panelX + 500, ly + 22, avgTxt);
    ly += 26;
  end;

  // Мини-график: средняя реакция по последним играм целиком (не по
  // отдельным кликам, как график на главном меню — другой смысл, поэтому
  // отдельная картинка, а не переиспользование DrawGraph)
  Font.Size := 12;
  Font.Color := ARGB(200, 200, 200, 200);
  DrawTextCentered(panelX + 20, ly + 8, panelX + 500, ly + 26, 'Средняя реакция по последним играм:');
  ly += 32;

  graphX := panelX + 20;
  graphY := ly;
  graphW := 480;
  graphH := 140;
  Brush.Color := ARGB(120, 30, 30, 35);
  FillRoundRect(graphX, graphY, graphX + graphW, graphY + graphH, 6, 6);

  if LoadedSessionCount > 0 then
  begin
    cnt := LoadedSessionCount;
    if cnt > 20 then cnt := 20;
    startIdx := LoadedSessionCount - cnt;

    maxVal := 100;
    for i := startIdx to LoadedSessionCount - 1 do
      if Round(LoadedSessionAvgReaction[i]) > maxVal then
        maxVal := Round(LoadedSessionAvgReaction[i]);

    SetLength(points, cnt);
    for i := 0 to cnt - 1 do
    begin
      points[i].X := graphX + Round(i * graphW / (cnt - 1 + 0.0001));
      val := Round(LoadedSessionAvgReaction[startIdx + i]);
      points[i].Y := graphY + graphH - Round(val * (graphH - 10) / maxVal) - 5;
    end;
    if cnt >= 2 then
      DrawSmoothCurve(points, ARGB(255, 97, 207, 255), 2)
    else
    begin
      Brush.Color := ARGB(255, 97, 207, 255);
      FillCircle(points[0].X, points[0].Y, 3);
    end;
  end;

  Brush.Color := ARGB(160, 90, 90, 95);
  Pen.Color := ARGB(0, 0, 0, 0);
  FillRoundRect(panelX + 20, panelY + 410, panelX + 240, panelY + 442, 8, 8);
  Font.Size := 12;
  Font.Color := clWhite;
  DrawTextCentered(panelX + 20, panelY + 410, panelX + 240, panelY + 442, 'Экспорт в CSV');

  Font.Size := 10;
  Font.Color := ARGB(170, 180, 180, 180);
  DrawTextCentered(panelX + 250, panelY + 410, panelX + 500, panelY + 442, SessionsFileName);

  Font.Size := 11;
  Font.Color := ARGB(180, 180, 180, 180);
  DrawTextCentered(panelX, panelY + 436, panelX + 520, panelY + 454, 'Клик вне панели — закрыть');
end;

procedure RenderProfilesOverlay;
var
  panelX, panelY, i, py: integer;
begin
  Brush.Color := ARGB(210, 15, 15, 18);
  FillRoundRect(0, 0, W, H, 1, 1);

  panelX := ProfilesPanelX;
  panelY := ProfilesPanelY;

  Brush.Color := ARGB(235, 45, 45, 50);
  Pen.Color := ARGB(255, 90, 90, 95);
  Pen.Width := 1;
  FillRoundRect(panelX, panelY, panelX + 360, panelY + 320, 14, 14);

  Font.Size := 20;
  Font.Color := clWhite;
  DrawTextCentered(panelX, panelY + 14, panelX + 360, panelY + 44, 'Профили');

  for i := 1 to MAX_PROFILES do
  begin
    py := panelY + 60 + (i - 1) * 50;
    if i = CurrentProfile then
    begin
      Brush.Color := ARGB(200, 97, 207, 255);
      Pen.Color := clWhite;
      Pen.Width := 2;
    end
    else
    begin
      Brush.Color := ARGB(90, 255, 255, 255);
      Pen.Color := ARGB(0, 0, 0, 0);
      Pen.Width := 1;
    end;
    FillRoundRect(panelX + 20, py, panelX + 340, py + 40, 8, 8);

    Font.Size := 14;
    Font.Color := clWhite;
    DrawTextCentered(panelX + 20, py, panelX + 340, py + 40, 'Профиль ' + i.ToString);
  end;

  Font.Size := 11;
  Font.Color := ARGB(180, 180, 180, 180);
  DrawTextCentered(panelX, panelY + 320 - 24, panelX + 360, panelY + 320 - 6, 'Клик по профилю — переключить');
end;

procedure RenderTutorialOverlay;
var
  panelX, panelY, panelW, panelH: integer;
begin
  Brush.Color := ARGB(225, 15, 15, 18);
  FillRoundRect(0, 0, W, H, 1, 1);

  panelW := 480; panelH := 220;
  panelX := (W - panelW) div 2;
  panelY := (H - panelH) div 2;

  Brush.Color := ARGB(240, 45, 45, 50);
  Pen.Color := ARGB(255, 97, 207, 255);
  Pen.Width := 2;
  FillRoundRect(panelX, panelY, panelX + panelW, panelY + panelH, 14, 14);

  Font.Size := 16;
  Font.Color := clWhite;
  DrawTextCentered(panelX + 20, panelY + 30, panelX + panelW - 20, panelY + 130, TutorialTexts[TutorialStep]);

  Font.Size := 12;
  Font.Color := ARGB(200, 200, 200, 200);
  DrawTextCentered(panelX, panelY + panelH - 50, panelX + panelW, panelY + panelH - 30,
    (TutorialStep + 1).ToString + ' / 4');

  Font.Color := ARGB(180, 180, 180, 180);
  DrawTextCentered(panelX, panelY + panelH - 26, panelX + panelW, panelY + panelH - 8,
    IfThen(TutorialStep < 3, 'Клик — далее', 'Клик — начать!'));
end;

procedure DrawMiniButton(x, y, x1, y1: integer; btext: string);
begin
  Pen.Color := ARGB(200, 255, 255, 255);
  DrawRoundRect(x, y, x1, y1, 10, 10);
  DrawTextCentered(x, y, x1, y1, btext);
end;

procedure RenderMainMenu;
var
  animIdx, menuDelta: integer;
begin
  ClearWindow(ThemeBg); // в v4 этого вызова не было — экран не
                        // очищался, и меню "смазывалось" кадр за кадром
  DrawParticles;

  // Анимация меню считается от реального времени (Milliseconds), а не
  // от количества итераций цикла — иначе скорость анимации плавает
  // в зависимости от того, насколько быстро крутится цикл на конкретном
  // железе (в v4 было MenuAnimTimer += 16 на каждую итерацию).
  menuDelta := Milliseconds - LastMenuAnimTick;
  if (menuDelta < 0) or (menuDelta > 200) then menuDelta := 16;
  MenuAnimTimer += menuDelta;
  LastMenuAnimTick := Milliseconds;

  UpdateMenuInput;

  animIdx := SelectedModeIndex;
  if SelectedModeIndex >= 0 then
    animIdx := SelectedModeIndex;

  DrawModePlate(PLATE_X, PLATE_Y, PLATE_W, PLATE_H, animIdx);
  DrawModeButtons;
  DrawModeDescription;

  DrawGraph;

  Font.Size := 12;
  Font.Color := ARGB(200, 200, 200, 200);
  var bottomY := H - 50;
  var colWidth := W div 5;
  DrawTextCentered(0, bottomY - 10, colWidth, bottomY + 10, 'Best Score: ' + Stats.BestScore.ToString);
  DrawTextCentered(colWidth, bottomY - 10, colWidth * 2, bottomY + 10, 'Best Acc: ' + Format('{0:0.0}%', Stats.BestAccuracy));
  DrawTextCentered(colWidth * 2, bottomY - 10, colWidth * 3, bottomY + 10, 'Best Prec: ' + Format('{0:0.0}%', Stats.BestPrecision));
  DrawTextCentered(colWidth * 3, bottomY - 10, colWidth * 4, bottomY + 10, 'Best Avg: ' + Format('{0:0.0}ms', Stats.BestAvgReaction));
  DrawTextCentered(colWidth * 4, bottomY - 10, W, bottomY + 10, 'Games: ' + Stats.GamesPlayed.ToString);

  Font.Size := 10;
  Font.Color := ARGB(170, 200, 200, 200);
  DrawMiniButton(10, H - 35, 100, H - 10, 'Справка');
  DrawMiniButton(110, H - 35, 200, H - 10, 'Настройки');
  DrawMiniButton(210, H - 35, 300, H - 10, 'Рекорды');
  DrawMiniButton(310, H - 35, 400, H - 10, 'Награды');
  DrawMiniButton(410, H - 35, 500, H - 10, 'История');
  DrawMiniButton(510, H - 35, 600, H - 10, 'Профиль ' + CurrentProfile.ToString);

  Font.Color := ARGB(150, 200, 200, 200);
  var statusText := IfThen(ShowMainMenuEffects, 'Effects: ON', 'Effects: OFF');
  DrawTextCentered(W - 120, H - 20, W - 10, H - 10, statusText);

  RenderToast;

  if ShowHelp then
    RenderHelpOverlay;
  if ShowSettings then
    RenderSettingsOverlay;
  if ShowLeaderboard then
    RenderLeaderboardOverlay;
  if ShowAchievements then
    RenderAchievementsOverlay;
  if ShowHistory then
    RenderHistoryOverlay;
  if ShowProfiles then
    RenderProfilesOverlay;
  if ShowTutorial then
    RenderTutorialOverlay;
end;

// ----- Тепловая карта промахов текущего забега (компактно, угол экрана) -----
procedure RenderMissHeatmap;
var
  cx, cy, maxCount, cellW, cellH, gx, gy, alpha: integer;
  originX, originY: integer;
begin
  maxCount := 1;
  for cx := 0 to HEATMAP_COLS - 1 do
    for cy := 0 to HEATMAP_ROWS - 1 do
      if MissHeatmap[cx][cy] > maxCount then
        maxCount := MissHeatmap[cx][cy];

  originX := W - 220;
  originY := 20;
  cellW := 200 div HEATMAP_COLS;
  cellH := 100 div HEATMAP_ROWS;

  Font.Size := 10;
  Font.Color := ARGB(180, 200, 200, 200);
  DrawTextCentered(originX, originY - 18, originX + 200, originY, 'Промахи (последний забег)');

  Brush.Color := ARGB(80, 30, 30, 35);
  FillRoundRect(originX, originY, originX + 200, originY + 100, 4, 4);

  for cx := 0 to HEATMAP_COLS - 1 do
    for cy := 0 to HEATMAP_ROWS - 1 do
    begin
      if MissHeatmap[cx][cy] > 0 then
      begin
        alpha := round(220 * (MissHeatmap[cx][cy] / maxCount));
        if alpha < 40 then alpha := 40;
        gx := originX + cx * cellW;
        gy := originY + cy * cellH;
        Brush.Color := ARGB(alpha, 255, 90, 70);
        FillRoundRect(gx, gy, gx + cellW, gy + cellH, 1, 1);
      end;
    end;
end;

procedure RenderGameOver;
var
  acc, avgReaction, precision: real;
  y: integer;
  title: string;
begin
  ClearWindow(ThemeBg);

  Font.Size := 44;
  Font.Color := clWhite;
  title := IfThen(Game.DiedToBomb, 'ПОДРЫВ!', 'GAME OVER');
  DrawTextCentered(0, H div 2 - 60, W, H div 2 + 20, title);

  if Game.DiedToBomb then
  begin
    Font.Size := 14;
    Font.Color := ARGB(220, 255, 120, 100);
    DrawTextCentered(0, H div 2 + 24, W, H div 2 + 44, 'Кликнул по бомбе — мгновенный конец забега');
    Font.Color := clWhite;
  end;

  Font.Size := 18;
  DrawTextCentered(0, H div 2 + 60, W, H div 2 + 90, 'Press SPACE to continue');

  Font.Size := 15;
  y := H div 2 + 120;
  DrawTextCentered(0, y, W, y + 24, 'Score: ' + Game.Score.ToString);
  y += 26;

  if Game.TotalShots > 0 then
    acc := Game.Hits / Game.TotalShots * 100
  else
    acc := 0;
  DrawTextCentered(0, y, W, y + 24, 'Accuracy: ' + Format('{0:0.0}', acc) + '%');
  y += 26;

  if Game.Hits > 0 then
  begin
    avgReaction := Game.TotalReactionTime / Game.Hits;
    precision := Game.TotalPrecisionSum / Game.Hits * 100;
  end
  else
  begin
    avgReaction := 0;
    precision := 0;
  end;
  DrawTextCentered(0, y, W, y + 24, 'Avg Reaction: ' + Format('{0:0.0}ms', avgReaction));
  y += 26;

  DrawTextCentered(0, y, W, y + 24, 'Precision: ' + Format('{0:0.0}', precision) + '%');
  y += 26;

  DrawTextCentered(0, y, W, y + 24, 'Hits: ' + Game.Hits.ToString + '  Misses: ' + Game.WrongClicks.ToString);
  y += 26;

  DrawTextCentered(0, y, W, y + 24, 'Max Combo: x' + Game.MaxCombo.ToString);

  RenderMissHeatmap;
  RenderToast;
end;

// ----- Обработка клика (общая для ЛКМ и ПКМ) -----
// Раньше это было двумя копиями одного и того же кода, вторая копия
// (ПКМ) вдобавок вызывалась только в Dual Mode — из-за этого правый клик
// в Classic Mode вообще ничего не делал, даже не считался промахом.
// GetHitInfo сам знает, валидна ли кнопка для текущего режима, поэтому
// здесь достаточно вызывать её для обеих кнопок всегда.
procedure ProcessClick(button: integer);
var
  i, points: integer;
  p: Point;
  hit: boolean;
  accuracy, mult: real;
  info: THitInfo;
  reaction: integer;
  hitColor: Color;
  ctx: TCircleContext;
begin
  Inc(Game.TotalShots);
  p := GetMousePos;
  hit := false;
  accuracy := 0;
  ctx := MakeCircleContext;

  for i := 0 to Game.CircleCount - 1 do
  begin
    info := Game.Circles[i].GetHitInfo(p.X, p.Y, button, ctx);
    if info.IsBombHit then
    begin
      // Бомба — мгновенный конец забега, обычная обработка клика тут не
      // нужна. Звук конца игры не дублируем здесь — GameLoop сам сыграет
      // его один раз, когда увидит Game.Fail на следующей проверке.
      Game.Fail := true;
      Game.DiedToBomb := true;
      SpawnEffect(5, 0, 0, 0, 0, 350, clWhite, '');
      Exit;
    end;
    if info.Hit then
    begin
      hit := true;
      accuracy := info.Accuracy;
      Inc(Game.Combo);
      if Game.Combo > Game.MaxCombo then Game.MaxCombo := Game.Combo;

      // Множитель очков за серию попаданий подряд без промаха
      if Game.Combo >= 10 then mult := 2.0
      else if Game.Combo >= 5 then mult := 1.5
      else mult := 1.0;

      // Очки зависят от масштаба именно этой цели — меньше круг, больше
      // очков. 13 подобрано так, чтобы очки укладывались примерно в
      // 10..20 за попадание на всём диапазоне MIN/MAX_SIZE_FACTOR, до
      // умножения на комбо-множитель.
      points := Round((13 / Game.Circles[i].SizeFactor) * mult);
      hitColor := CircleColors[Game.Circles[i].CType];
      Game.Score += points;
      Inc(Game.Hits); // чистое число попаданий — отдельно от очков (Score)
      reaction := Game.TryTime - Game.Circles[i].SpawnTime;
      Game.TotalReactionTime += reaction;
      AddReactionTime(reaction);
      Game.Circles[i].Spawn(Game.Circles[i].X, Game.Circles[i].Y, ctx);
      SpawnHitEffects(p.X, p.Y, hitColor, points);
      PlayHitSound;

      if accuracy >= BOMB_ACCURACY_THRESHOLD then
      begin
        // Визуальный акцент вместо реального замедления времени — не
        // трогаем FrameTime/TryTime, от них зависит замер реакции
        SpawnEffect(4, 0, 0, 0, 0, 200, clWhite, '');
        UnlockAchievement(1); // "В яблочко"
      end;
      if reaction < 150 then UnlockAchievement(2);   // "Молниеносно"
      if Game.Combo >= 100 then UnlockAchievement(3); // "Комбо x100"
      if Game.Score >= 1000 then UnlockAchievement(4); // "Тысяча"

      Break;
    end;
  end;
  Game.TotalPrecisionSum += accuracy;
  if not hit then
  begin
    Game.Combo := 0;
    Inc(Game.Loses);
    Inc(Game.WrongClicks);
    PlayMissSound;
    RecordMiss(p.X, p.Y);
  end;
end;

// ----- Обновление игры -----
procedure UpdateGame;
var
  i, points: integer;
  ctx: TCircleContext;
  mp: Point;
  completed: boolean;
begin
  Game.FrameTime := MillisecondsDelta;
  Game.TryTime += Game.FrameTime;
  Game.tempFPS += 1;
  Game.LastTime -= Game.FrameTime;
  if Game.LastTime <= 0 then
  begin
    Game.FPS := Game.tempFPS;
    Game.tempFPS := 0;
    Game.LastTime := 1000;
  end;

  UpdateCursorTrail;
  ctx := MakeCircleContext;

  for i := 0 to Game.CircleCount - 1 do
  begin
    Game.Circles[i].Update(Game.FrameTime, Game.Loses, ctx);
    if Game.Circles[i].Time < 0 then
      Game.Circles[i].Spawn(Game.Circles[i].X, Game.Circles[i].Y, ctx);
  end;

  if Game.GameMode = gmTracking then
  begin
    // Наводка вместо клика: прогресс растёт, пока курсор на цели (см.
    // TCircle.UpdateHover). Reaction/Precision тут не считаем — нет
    // дискретного момента клика, к которому их можно было бы привязать;
    // Score начисляем отдельно, Hits/TotalShots не трогаем, чтобы не
    // размывать Accuracy/Avg — они остаются метриками кликовых режимов.
    mp := GetMousePos;
    for i := 0 to Game.CircleCount - 1 do
    begin
      completed := false;
      Game.Circles[i].UpdateHover(mp.X, mp.Y, Game.FrameTime, ctx, completed);
      if completed then
      begin
        points := Round(13 / Game.Circles[i].SizeFactor);
        Game.Score += points;
        SpawnHitEffects(Game.Circles[i].X, Game.Circles[i].Y, CircleColors[Game.Circles[i].CType], points);
        PlayHitSound;
        Game.Circles[i].Spawn(Game.Circles[i].X, Game.Circles[i].Y, ctx);
      end;
    end;
  end
  else
  begin
    while IsMouseButtonPressed(1) do
    begin
      ProcessClick(1);
      ClearMouseButtonPressed(1);
    end;

    while IsMouseButtonPressed(2) do
    begin
      ProcessClick(2);
      ClearMouseButtonPressed(2);
    end;
  end;

  if Game.Loses >= Game.MaxLoses then Game.Fail := true;
end;

procedure RenderPauseOverlay;
begin
  Brush.Color := ARGB(150, 15, 15, 18);
  FillRoundRect(0, 0, W, H, 0, 0);

  Font.Size := 40;
  Font.Color := clWhite;
  DrawTextCentered(0, H div 2 - 70, W, H div 2 - 20, 'PAUSE');

  Font.Size := 14;
  Font.Color := ARGB(220, 210, 210, 210);
  DrawTextCentered(0, H div 2 + 10, W, H div 2 + 30, 'P — продолжить');
  DrawTextCentered(0, H div 2 + 34, W, H div 2 + 54, 'R — заново    M — в меню');
end;

// Считает итог забега, обновляет лучшие результаты и сохраняет всё на
// диск (автосохранение после каждой завершённой игры). Screen не трогает —
// куда идти дальше, решает вызывающий код: обычный конец игры уходит на
// scGameOver, выход по клавише M — сразу на scMainMenu.
procedure FinishGameAndSaveStats;
var
  acc, avgReaction, precision: real;
  i: integer;
begin
  if Game.TotalShots > 0 then
    acc := Game.Hits / Game.TotalShots * 100
  else
    acc := 0;
  if Game.Hits > 0 then
  begin
    avgReaction := Game.TotalReactionTime / Game.Hits;
    precision := Game.TotalPrecisionSum / Game.Hits * 100;
  end
  else
  begin
    avgReaction := 0;
    precision := 0;
  end;

  if Game.Score > Stats.BestScore then Stats.BestScore := Game.Score;
  if acc > Stats.BestAccuracy then Stats.BestAccuracy := acc;
  if (Game.Hits > 0) and (precision > Stats.BestPrecision) then
    Stats.BestPrecision := precision;
  if (Game.Hits > 0) and ((Stats.BestAvgReaction = 0) or (avgReaction < Stats.BestAvgReaction)) then
    Stats.BestAvgReaction := avgReaction;
  Inc(Stats.GamesPlayed);

  TryAddToLeaderboard(Game.GameMode, Game.Score, acc);
  LogSession(Game.GameMode, Game.Score, acc, avgReaction, Game.Hits, Game.WrongClicks, Game.MaxCombo);

  if Stats.GamesPlayed = 1 then UnlockAchievement(0); // "Первая игра"
  if (Game.TotalShots >= 20) and (acc >= 90) then UnlockAchievement(5); // "Снайпер"
  if Stats.GamesPlayed >= 1000 then UnlockAchievement(6); // "Ветеран"
  if (Game.GameMode = gmDual) and (Game.Score >= 500) then UnlockAchievement(7); // "Мастер Dual"

  for i := 0 to Game.ReactionCount - 1 do
  begin
    SavedReactionHistory[SavedReactionIndex] := Game.ReactionHistory[i];
    SavedReactionIndex := (SavedReactionIndex + 1) mod 50;
    if SavedReactionCount < 50 then
      Inc(SavedReactionCount);
  end;

  SaveStats;
end;

// ----- Экранные состояния -----
procedure StartScreen;
begin
  Screen := scMainMenu;
  InitParticles;
  LastMenuAnimTick := Milliseconds;
  MenuEnterTick := Milliseconds;
  while Screen = scMainMenu do
  begin
    for var i := 0 to PARTICLE_COUNT-1 do
      Particles[i].Update(W, H);

    // Клики по меню (выбор режима / запуск / переключение эффектов /
    // справка) обрабатывает UpdateMenuInput внутри RenderMainMenu — старой
    // кнопочной раскладки (Buttons/TButton) больше нет, её сменила
    // плашка с превью режимов.
    RenderMainMenu;
    Redraw;

    if ShowHelp and IsKeyPressed(VK_ESCAPE) then
      ShowHelp := false;
    if ShowSettings and IsKeyPressed(VK_ESCAPE) then
      ShowSettings := false;
    if ShowLeaderboard and IsKeyPressed(VK_ESCAPE) then
      ShowLeaderboard := false;
    if ShowAchievements and IsKeyPressed(VK_ESCAPE) then
      ShowAchievements := false;
    if ShowHistory and IsKeyPressed(VK_ESCAPE) then
      ShowHistory := false;
    if ShowProfiles and IsKeyPressed(VK_ESCAPE) then
      ShowProfiles := false;
    // Туториал по Esc пропускается так же, как и по клику — сразу
    // помечается просмотренным, чтобы не показывался повторно
    if ShowTutorial and IsKeyPressed(VK_ESCAPE) then
    begin
      ShowTutorial := false;
      Stats.TutorialSeen := true;
      SaveStats;
    end;

    UpdateInput;
    LimitFrameRate;
  end;
end;

procedure CountdownScreen;
begin
  Game.CountdownValue := 3;
  Game.CountdownStartTime := Milliseconds;
  while Screen = scCountdown do
  begin
    if IsKeyPressed(KEY_M) then
    begin
      Screen := scMainMenu;
      Break;
    end;

    var elapsed := Milliseconds - Game.CountdownStartTime;
    if elapsed >= 1000 then
    begin
      Dec(Game.CountdownValue);
      Game.CountdownStartTime := Milliseconds;
      if Game.CountdownValue <= 0 then
      begin
        Screen := scGame;
        InitGameState;
        Break;
      end;
    end;
    ClearWindow(ThemeBg);
    Font.Size := 20;
    DrawTextCentered(0, H div 2 - 140, W, H div 2 - 120, Modes[SelectedModeIndex].Name);
    Font.Size := 120;
    DrawTextCentered(0, H div 2 - 100, W, H div 2 + 100, Game.CountdownValue.ToString);
    Font.Size := 20;
    DrawTextCentered(0, H div 2 + 120, W, H div 2 + 140, 'Get ready!');
    Redraw;
    UpdateInput;
    LimitFrameRate;
  end;
end;

procedure GameLoop;
begin
  Screen := scGame;
  Game.Paused := false;
  // Сбрасываем накопленную дельту: время, проведённое в меню и на
  // countdown, не должно попасть в тайминг первой цели этой игры.
  MillisecondsDelta;
  while Screen = scGame do
  begin
    if IsKeyPressed(KEY_P) then
    begin
      Game.Paused := not Game.Paused;
      if not Game.Paused then
      begin
        MillisecondsDelta; // не даём времени паузы попасть в тайминг следующей цели
        ClearMouseButtonPressed(1);
        ClearMouseButtonPressed(2);
      end;
    end;

    if not Game.Paused then
      UpdateGame;

    RenderGame;
    if Game.Paused then
      RenderPauseOverlay;
    Redraw;

    if IsKeyPressed(KEY_R) then
    begin
      // Game.GameMode не трогаем — перезапускаем тот же режим.
      // Текущий (незавершённый) забег в статистику не идёт.
      Screen := scCountdown;
      Break;
    end;

    if IsKeyPressed(KEY_M) then
    begin
      FinishGameAndSaveStats;
      Screen := scMainMenu;
      Break;
    end;

    if IsKeyPressed(VK_ESCAPE) then
      Game.Fail := true;

    if Game.Fail then
    begin
      FinishGameAndSaveStats;
      PlayGameOverSound;
      Screen := scGameOver;
      Break;
    end;

    UpdateInput;
    LimitFrameRate;
  end;
end;

procedure GameOverScreen;
begin
  Screen := scGameOver;
  while Screen = scGameOver do
  begin
    RenderGameOver;
    Redraw;
    if IsKeyPressed(VK_SPACE) or IsKeyPressed(KEY_M) then
    begin
      Screen := scMainMenu;
      Break;
    end;
    if IsKeyPressed(KEY_R) then
    begin
      Screen := scCountdown; // тот же режим, что был только что сыгран
      Break;
    end;
    UpdateInput;
    LimitFrameRate;
  end;
end;

// ----- Основная программа -----
begin
  SetWindowIsFixedSize(true); // раскладка UI жёстко завязана на W x H
  SetWindowSize(W, H);
  SetWindowCaption('Aim Trainer');
  CenterWindow;
  LockDrawing;
  Font.Name := 'Consolas';
  Font.Size := 14;
  Randomize;

  SetLength(CircleColors, 2);

  InitInput;
  ShowMainMenuEffects := true;
  ShowHelp := false;
  ShowSettings := false;
  ShowLeaderboard := false;
  ShowAchievements := false;
  ShowHistory := false;
  ShowProfiles := false;
  NextEffectSlot := 0;
  TrailNext := 0;
  CurrentProfile := 1; // LoadStats читает файл конкретно этого профиля
  InitThemes;
  InitAchievements;
  InitTutorialTexts;
  LoadStats;       // может переопределить CurrentTheme и AchievementUnlocked
  ApplyTheme;       // применяем загруженную (или дефолтную) тему к CircleColors
  InitModes; // один раз на весь запуск программы, не на каждую игру
  LastFrameTick := Milliseconds;

  // Туториал — один раз, при самом первом запуске (пока не сыграна ни
  // одна игра и явно не был просмотрен/пропущен раньше)
  ShowTutorial := not Stats.TutorialSeen;
  TutorialStep := 0;

  Screen := scMainMenu;
  while True do
  begin
    case Screen of
      scMainMenu:  StartScreen;
      scCountdown: CountdownScreen;
      scGame:      GameLoop;
      scGameOver:  GameOverScreen;
    end;
  end;
end.
