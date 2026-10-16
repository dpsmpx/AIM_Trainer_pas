unit AimCircle;

// ============================================================================
//  Логика мишени (TCircle), вынесенная из основного файла отдельным модулем.
//  Синтаксис unit/interface/implementation — по образцу уже работающих в
//  проекте Base.pas и Control_AI.pas.
//
//  TCircle раньше читал состояние игры (MaxRadius/MaxCTime/CCTime/GameMode/
//  TryTime) напрямую из глобальной переменной Game основной программы. Модуль
//  так не может — Game объявлена в программе, которая ПОДключает этот модуль,
//  а не наоборот. Поэтому все нужные значения собраны в TCircleContext и
//  передаются явным параметром в каждый метод; вызывающая сторона (основной
//  файл) строит контекст из своего Game одной строкой перед вызовом.
// ============================================================================

interface

uses GraphABC;

type
  // Tracking и Flick добавлены в этап 4 — раньше существовали только как
  // пункты меню без реальной механики (см. Implemented в основном файле)
  TGameMode = (gmClassic, gmDual, gmTracking, gmFlick);

  THitInfo = record
    Hit: boolean;
    Accuracy: real;
    IsBombHit: boolean;
  end;

  TCircleContext = record
    W, H: integer;
    MaxRadius: integer;
    MaxCTime: integer;
    CCTime: integer;
    TryTime: integer;
    GameMode: TGameMode;
  end;

  TCircle = record
    X, Y: integer;
    Time: integer;
    SpawnTime: integer;
    CType: integer;
    SizeFactor: real;   // случайный масштаб конкретной цели: меньше круг — больше очков
    IsBomb: boolean;    // клик — мгновенный Game Over; не тронул — исчезает без штрафа
    VX, VY: real;       // скорость движения, только для Tracking (px/мс)
    HoverMs: integer;   // накопленное время наводки, только для Tracking
    procedure Spawn(const PrevX, PrevY: integer; const ctx: TCircleContext);
    procedure Update(const FrameTime: integer; var Loses: integer; const ctx: TCircleContext);
    function CurrentRadius(const ctx: TCircleContext): integer;
    procedure Render(const ctx: TCircleContext; const colors: array of Color);
    function GetHitInfo(const mx, my, button: integer; const ctx: TCircleContext): THitInfo;
    // Только для Tracking: растёт, пока курсор на цели, тает вдвое быстрее,
    // когда курсор снят. completed=true — цель "докручена" и засчитана.
    procedure UpdateHover(const mx, my, FrameTime: integer; const ctx: TCircleContext; var completed: boolean);
  end;

implementation

const
  MIN_SIZE_FACTOR = 0.65;
  MAX_SIZE_FACTOR = 1.35;
  BOMB_CHANCE = 0.15;
  TRACKING_HOLD_MS = 900;

procedure TCircle.Spawn(const PrevX, PrevY: integer; const ctx: TCircleContext);
var
  minDistSq, dx, dy, distSq, attempts, topReserve, effR: integer;
  angle, speed: real;
begin
  // Сначала масштаб — от него зависят и границы спавна (effR), и итоговый
  // радиус, который потом использует CurrentRadius.
  SizeFactor := MIN_SIZE_FACTOR + Random * (MAX_SIZE_FACTOR - MIN_SIZE_FACTOR);
  effR := Round(ctx.MaxRadius * SizeFactor);

  topReserve := 0;
  if ctx.GameMode = gmDual then topReserve := 50; // место под подсказку цвет→кнопка
  attempts := 0;
  repeat
    X := Random(effR, ctx.W - effR);
    Y := Random(effR + topReserve, ctx.H - effR - 60);
    dx := X - PrevX;
    dy := Y - PrevY;
    distSq := dx*dx + dy*dy;
    minDistSq := (effR * 2) * (effR * 2);
    Inc(attempts);
  until (distSq >= minDistSq) or (attempts > 100);
  Time := ctx.MaxCTime;
  SpawnTime := ctx.TryTime;
  if ctx.GameMode = gmDual then CType := Random(0, 1)
  else CType := 0;

  // Бомбы: не в Tracking (там и так нужно постоянно следить за курсором —
  // ещё и "не трогай" запутает механику), не на первую цель забега
  // (TryTime=0 только у самого первого спавна), дальше — фиксированный шанс
  IsBomb := (ctx.TryTime > 0) and (ctx.GameMode <> gmTracking) and (Random < BOMB_CHANCE);

  HoverMs := 0;
  if ctx.GameMode = gmTracking then
  begin
    angle := Random * 2 * Pi;
    speed := 60 + Random(60); // условных px/сек
    VX := Cos(angle) * speed / 1000; // переводим в px/мс — прямое умножение на FrameTime
    VY := Sin(angle) * speed / 1000;
  end
  else
  begin
    VX := 0;
    VY := 0;
  end;
end;

procedure TCircle.Update(const FrameTime: integer; var Loses: integer; const ctx: TCircleContext);
begin
  if ctx.GameMode = gmTracking then
  begin
    X += Round(VX * FrameTime);
    Y += Round(VY * FrameTime);
    // Отскок от границ — тот же принцип, что у частиц на главном меню
    if (X < ctx.MaxRadius) or (X > ctx.W - ctx.MaxRadius) then VX := -VX;
    if (Y < ctx.MaxRadius) or (Y > ctx.H - ctx.MaxRadius - 60) then VY := -VY;
    if X < ctx.MaxRadius then X := ctx.MaxRadius;
    if X > ctx.W - ctx.MaxRadius then X := ctx.W - ctx.MaxRadius;
    if Y < ctx.MaxRadius then Y := ctx.MaxRadius;
    if Y > ctx.H - ctx.MaxRadius - 60 then Y := ctx.H - ctx.MaxRadius - 60;
  end;

  Time -= FrameTime;
  // Бомба, которую не тронули — просто гаснет, жизнь за неё не снимается
  if (Time < 0) and not IsBomb then Inc(Loses);
end;

// Текущий радиус цели. Classic/Dual/Flick — анимация роста/сжатия с учётом
// персонального SizeFactor. Tracking — стабильный размер: за движущейся
// целью и так нужно уследить, скачущий радиус только мешал бы.
function TCircle.CurrentRadius(const ctx: TCircleContext): integer;
var
  peak: integer;
begin
  peak := Round(ctx.MaxRadius * SizeFactor);
  if ctx.GameMode = gmTracking then
  begin
    Result := peak;
    Exit;
  end;
  if Time > ctx.CCTime then
    Result := peak - Round(peak * ((Time - ctx.CCTime) / ctx.CCTime))
  else
    Result := Round(peak * (Time / ctx.CCTime));
end;

procedure TCircle.Render(const ctx: TCircleContext; const colors: array of Color);
var
  r, cross, progAlpha: integer;
begin
  if Time >= ctx.MaxCTime then Exit;
  r := CurrentRadius(ctx);
  if r <= 0 then Exit;

  if IsBomb then
  begin
    Brush.Color := ARGB(230, 40, 20, 20);
    FillCircle(X, Y, r);
    Pen.Color := clWhite;
    Pen.Width := 3;
    cross := Round(r * 0.5);
    Line(X - cross, Y - cross, X + cross, Y + cross);
    Line(X - cross, Y + cross, X + cross, Y - cross);
    Exit;
  end;

  Brush.Color := colors[CType];
  FillCircle(X, Y, r);

  if ctx.GameMode = gmTracking then
  begin
    // Кольцо-индикатор наводки: чем ближе к завершению, тем ярче
    if HoverMs > 0 then
    begin
      progAlpha := round(255 * (HoverMs / TRACKING_HOLD_MS));
      if progAlpha > 255 then progAlpha := 255;
      Pen.Color := ARGB(progAlpha, 255, 255, 255);
      Pen.Width := 3;
      Circle(X, Y, r + 6);
    end;
  end
  else if r > 8 then
  begin
    // Зоны точности: подсказка, докуда нужно докликивать ради Accuracy
    // 75%/50% (формула та же, что в GetHitInfo: 1 - dist/r)
    Pen.Color := ARGB(90, 255, 255, 255);
    Pen.Width := 1;
    Circle(X, Y, Round(r * 0.5));
    Circle(X, Y, Round(r * 0.25));
  end;
end;

function TCircle.GetHitInfo(const mx, my, button: integer; const ctx: TCircleContext): THitInfo;
var
  dx, dy, dist, r: integer;
begin
  Result.Hit := false;
  Result.Accuracy := 0;
  Result.IsBombHit := false;
  if (Time >= ctx.MaxCTime) or (Time < 0) then Exit;
  r := CurrentRadius(ctx);
  if r <= 0 then Exit;
  dx := mx - X; dy := my - Y;
  dist := Round(Sqrt(dx*dx + dy*dy));
  if dist <= r then
  begin
    if IsBomb then
    begin
      // Бомба реагирует на любую кнопку — сам факт клика уже плохая идея
      Result.Hit := true;
      Result.IsBombHit := true;
      Exit;
    end;
    if ctx.GameMode = gmClassic then
      Result.Hit := (button = 1)
    else
      Result.Hit := ((CType = 0) and (button = 1)) or ((CType = 1) and (button = 2));
    if Result.Hit then
    begin
      Result.Accuracy := 1 - dist / r;
      if Result.Accuracy < 0 then Result.Accuracy := 0;
      if Result.Accuracy > 1 then Result.Accuracy := 1;
    end;
  end;
end;

procedure TCircle.UpdateHover(const mx, my, FrameTime: integer; const ctx: TCircleContext; var completed: boolean);
var
  dx, dy, dist, r: integer;
begin
  completed := false;
  r := CurrentRadius(ctx);
  dx := mx - X;
  dy := my - Y;
  dist := Round(Sqrt(dx*dx + dy*dy));

  if dist <= r then
    HoverMs += FrameTime
  else
  begin
    HoverMs -= FrameTime * 2; // отпустил цель — прогресс тает вдвое быстрее, чем набирается
    if HoverMs < 0 then HoverMs := 0;
  end;

  if HoverMs >= TRACKING_HOLD_MS then
    completed := true;
end;

end.
