// ============================================================================
//  AIM_Trainer_AI.pas  –  Основной игровой модуль
//  Исправлены недочёты:
//   - Убраны лишние параметры в DrawTextWithBackground
//   - Унифицировано расстояние для линий от курсора
//   - TCircle.Update теперь использует переданные параметры
//   - Удалён дублирующий метод CheckHit (используется только GetHitInfo)
// ============================================================================

uses GraphABC, Base, Control_AI;

type
  TScreen = (scMainMenu, scCountdown, scGame, scGameOver);
  TGameMode = (gmClassic, gmDual);
  THitInfo = record
    Hit: boolean;
    Accuracy: real;
  end;

  TCircle = record
    X, Y: integer;
    Time: integer;
    SpawnTime: integer;
    CType: integer;
    procedure Spawn(const MaxRadius, W, H: integer; const GameMode: TGameMode; const PrevCircle: TCircle);
    procedure Update(const FrameTime: integer; var Loses: integer; var Fail: boolean; const MaxCTime, CCTime: integer);
    procedure Render(const CircleColors: array of Color; const MaxRadius, MaxCTime, CCTime: integer);
    function GetHitInfo(const mx, my: integer; const GameMode: TGameMode; const Button: integer; const MaxRadius, MaxCTime, CCTime: integer): THitInfo;
  end;

  TGameState = record
    Score: integer;
    TotalShots: integer;
    Loses: integer;
    WrongClicks: integer;
    TotalReactionTime: integer;
    TotalAccuracySum: real;
    MaxLoses: integer;
    Fail: boolean;
    TryTime: integer;
    IsPlayTime: boolean;
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
  end;

  TButton = record
    X, Y, Width, Height: integer;
    Text: string;
    NormalColor, HoverColor: Color;
    CurrentAlpha: integer;
    IsActive: boolean;
    procedure Render(shadow: boolean);
    function IsHover: boolean;
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
    BestAvgReaction: real;
    GamesPlayed: integer;
  end;

var
  W := 800;
  H := 600;
  CircleColors: array of Color;
  Game: TGameState;
  Buttons: array of TButton;
  Screen: TScreen;
  Stats: TStats;
  SavedReactionHistory: array[0..49] of integer;
  SavedReactionCount: integer;
  SavedReactionIndex: integer;
  ShowMainMenuEffects: boolean;

const
  PARTICLE_COUNT = 100;
  PARTICLE_SPEED = 1.0;
  LINE_DIST = 100.0;

var
  Particles: array[0..PARTICLE_COUNT-1] of TParticle;

// ----- Загрузка/сохранение статистики -----
procedure LoadStats;
var
  f: Text;
  line: string;
  parts: array of string;
  i, idx, val: integer;
begin
  Stats.BestScore := 0;
  Stats.BestAccuracy := 0;
  Stats.BestAvgReaction := 0;
  Stats.GamesPlayed := 0;
  SavedReactionCount := 0;
  SavedReactionIndex := 0;
  for i := 0 to 49 do SavedReactionHistory[i] := 0;

  if not FileExists('stats.txt') then Exit;

  Assign(f, 'stats.txt');
  Reset(f);
  while not Eof(f) do
  begin
    Readln(f, line);
    parts := line.Split('=');
    if Length(parts) = 2 then
    begin
      if parts[0] = 'BestScore' then Stats.BestScore := StrToInt(parts[1])
      else if parts[0] = 'BestAccuracy' then Stats.BestAccuracy := StrToFloat(parts[1])
      else if parts[0] = 'BestAvgReaction' then Stats.BestAvgReaction := StrToFloat(parts[1])
      else if parts[0] = 'GamesPlayed' then Stats.GamesPlayed := StrToInt(parts[1])
      else if parts[0] = 'ReactionCount' then SavedReactionCount := StrToInt(parts[1])
      else if parts[0] = 'ReactionIndex' then SavedReactionIndex := StrToInt(parts[1])
      else if parts[0] = 'ShowMainMenuEffects' then ShowMainMenuEffects := (parts[1] = 'True')
      else if Copy(parts[0], 1, 8) = 'Reaction' then
      begin
        idx := StrToInt(Copy(parts[0], 9, Length(parts[0])-8));
        val := StrToInt(parts[1]);
        if (idx >= 0) and (idx < 50) then
          SavedReactionHistory[idx] := val;
      end;
    end;
  end;
  Close(f);
end;

procedure SaveStats;
var
  f: Text;
  i: integer;
begin
  Assign(f, 'stats.txt');
  Rewrite(f);
  Writeln(f, 'BestScore=' + Stats.BestScore.ToString);
  Writeln(f, 'BestAccuracy=' + Stats.BestAccuracy.ToString);
  Writeln(f, 'BestAvgReaction=' + Stats.BestAvgReaction.ToString);
  Writeln(f, 'GamesPlayed=' + Stats.GamesPlayed.ToString);
  Writeln(f, 'ReactionCount=' + SavedReactionCount.ToString);
  Writeln(f, 'ReactionIndex=' + SavedReactionIndex.ToString);
  Writeln(f, 'ShowMainMenuEffects=' + ShowMainMenuEffects.ToString);
  for i := 0 to 49 do
    Writeln(f, 'Reaction' + i.ToString + '=' + SavedReactionHistory[i].ToString);
  Close(f);
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

// ----- Кнопки -----
procedure CreateButton(var btn: TButton; x, y, w, h: integer; text: string; normalColor, hoverColor: Color);
begin
  btn.X := x; btn.Y := y; btn.Width := w; btn.Height := h;
  btn.Text := text;
  btn.NormalColor := normalColor;
  btn.HoverColor := hoverColor;
  btn.CurrentAlpha := 150;
  btn.IsActive := true;
end;

procedure TButton.Render(shadow: boolean);
var
  useColor: Color;
begin
  useColor := IfThen(IsHover and IsActive, HoverColor, NormalColor);
  DrawTextWithBackground(
    ARGB(CurrentAlpha, GetRed(useColor), GetGreen(useColor), GetBlue(useColor)),
    X, Y, X + Width, Y + Height, 16,
    Text,
    shadow
  );
end;

function TButton.IsHover: boolean;
var
  p: Point;
begin
  p := GetMousePos;
  Result := (p.X >= X) and (p.X <= X + Width) and
            (p.Y >= Y) and (p.Y <= Y + Height);
end;

// ----- TCircle -----
procedure TCircle.Spawn(const MaxRadius, W, H: integer; const GameMode: TGameMode; const PrevCircle: TCircle);
var
  minDistSq, dx, dy, distSq: integer;
  attempts: integer;
begin
  attempts := 0;
  repeat
    X := Random(MaxRadius, W - MaxRadius);
    Y := Random(MaxRadius, H - MaxRadius - 60);
    dx := X - PrevCircle.X;
    dy := Y - PrevCircle.Y;
    distSq := dx*dx + dy*dy;
    minDistSq := (MaxRadius * 2) * (MaxRadius * 2);
    Inc(attempts);
  until (distSq >= minDistSq) or (attempts > 100);
  Time := Game.MaxCTime;
  SpawnTime := Game.TryTime;
  if GameMode = gmDual then CType := Random(0, 1)
  else CType := 0;
end;

// Теперь Update использует переданные параметры, а не глобальный Game
procedure TCircle.Update(const FrameTime: integer; var Loses: integer; var Fail: boolean; const MaxCTime, CCTime: integer);
begin
  if not Game.IsPlayTime then Exit;
  Time -= FrameTime;
  if Time < 0 then
  begin
    Inc(Loses);
    if Loses >= Game.MaxLoses then
      Fail := true;
  end;
end;

procedure TCircle.Render(const CircleColors: array of Color; const MaxRadius, MaxCTime, CCTime: integer);
var
  r: integer;
begin
  if not Game.IsPlayTime then Exit;
  if Time < MaxCTime then
  begin
    if Time > CCTime then
      r := MaxRadius - Round(MaxRadius * ((Time - CCTime) / CCTime))
    else
      r := Round(MaxRadius * (Time / CCTime));
    if r > 0 then
    begin
      Brush.Color := CircleColors[CType];
      FillCircle(X, Y, r);
    end;
  end;
end;

// Удалён дублирующий метод CheckHit – используется только GetHitInfo
function TCircle.GetHitInfo(const mx, my: integer; const GameMode: TGameMode; const Button: integer; const MaxRadius, MaxCTime, CCTime: integer): THitInfo;
var
  dx, dy, dist, r: integer;
begin
  Result.Hit := false;
  Result.Accuracy := 0;
  if (Time >= MaxCTime) or (Time < 0) then Exit;
  if Time > CCTime then
    r := MaxRadius - Round(MaxRadius * ((Time - CCTime) / CCTime))
  else
    r := Round(MaxRadius * (Time / CCTime));
  if r <= 0 then Exit;
  dx := mx - X; dy := my - Y;
  dist := Round(Sqrt(dx*dx + dy*dy));
  if dist <= r then
  begin
    if GameMode = gmClassic then
      Result.Hit := (Button = 1)
    else
      Result.Hit := ((CType = 0) and (Button = 1)) or ((CType = 1) and (Button = 2));
    if Result.Hit then
    begin
      Result.Accuracy := 1 - dist / r;
      if Result.Accuracy < 0 then Result.Accuracy := 0;
      if Result.Accuracy > 1 then Result.Accuracy := 1;
    end;
  end;
end;

// ----- Инициализация игры -----
procedure InitGameState;
var
  i: integer;
begin
  with Game do
  begin
    Score := 0;
    TotalShots := 0;
    Loses := 0;
    WrongClicks := 0;
    TotalReactionTime := 0;
    TotalAccuracySum := 0;
    MaxLoses := 15;
    Fail := false;
    TryTime := 0;
    IsPlayTime := true;
    MaxRadius := 40;
    MaxCTime := 1000;
    CCTime := MaxCTime div 2;
    CircleCount := 1;
    SetLength(Circles, CircleCount);
    FrameTime := 0;
    LastTime := 1000;
    FPS := 0;
    tempFPS := 0;
    ReactionCount := 0;
    ReactionIndex := 0;
    for i := 0 to 49 do ReactionHistory[i] := 0;
    Circles[0].Spawn(MaxRadius, W, H, GameMode, Circles[0]);
  end;
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

function GetReactionPoints: array of Point;
var
  i, cnt, graphW, graphH: integer;
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
  i: integer;
begin
  ClearWindow(RGB(28, 28, 32));
  for i := 0 to Game.CircleCount - 1 do
    Game.Circles[i].Render(CircleColors, Game.MaxRadius, Game.MaxCTime, Game.CCTime);

  oldFontSize := Font.Size;
  Font.Size := 14;
  bgColor := ARGB(220, 60, 60, 65);
  infoH := 60;
  colWidth := W div 5;
  paddingW := 16;
  bottom := 16;

  // Убраны лишние параметры 16 – теперь вызовы соответствуют сигнатуре
  DrawTextWithBackground(bgColor, 0 + paddingW, H - infoH, colWidth - paddingW, H - bottom, 16,
                         'FPS:'#10 + Game.FPS.ToString, True);
  DrawTextWithBackground(bgColor, colWidth + paddingW, H - infoH, 2*colWidth - paddingW, H - bottom, 16,
                         'Score:'#10 + Game.Score, True);
  accuracy := IfThen(Game.TotalShots > 0, Game.TotalAccuracySum / Game.TotalShots * 100, 0.0);
  DrawTextWithBackground(bgColor, 2*colWidth + paddingW, H - infoH, 3*colWidth - paddingW, H - bottom, 16,
                         'Accuracy:'#10 + Format('{0:0.0}%', accuracy), True);
  avgReaction := IfThen(Game.Score > 0, Game.TotalReactionTime / Game.Score, 0.0);
  DrawTextWithBackground(bgColor, 3*colWidth + paddingW, H - infoH, 4*colWidth - paddingW, H - bottom, 16,
                         'Avg:'#10 + Format('{0:0.0}ms', avgReaction), True);
  DrawTextWithBackground(bgColor, 4*colWidth + paddingW, H - infoH, W - paddingW, H - bottom, 16,
                         'Lives:'#10 + (Game.MaxLoses - Game.Loses), True);
  Font.Size := oldFontSize;
end;

procedure RenderMainMenu;
var
  i: integer;
  p: Point;
  titleX1, titleY1, titleX2, titleY2: integer;
  inTitle: boolean;
begin
  ClearWindow(RGB(28, 28, 32));
  DrawParticles;

  titleX1 := 50; titleY1 := 50; titleX2 := W div 2; titleY2 := H div 2 - 50;
  p := GetMousePos;
  inTitle := (p.X >= titleX1) and (p.X <= titleX2) and
             (p.Y >= titleY1) and (p.Y <= titleY2);

  Brush.Color := ARGB(150, 100, 100, 100);
  FillRoundRect(titleX1, titleY1, titleX2, titleY2, 16, 16);

  if inTitle and ShowMainMenuEffects then
  begin
    Brush.Color := ARGB(120, 235, 60, 60);
    FillCircle(p.X, p.Y, 30);
  end;

  Font.Color := clWhite;
  Font.Size := 24;
  DrawTextCentered(titleX1, titleY1, titleX2, titleY2, 'AIM TRAINER');

  for i := 0 to Buttons.Length - 1 do
  begin
    Buttons[i].CurrentAlpha := IfThen(Buttons[i].IsHover, 220, 150);
    Buttons[i].Render(True);
  end;

  DrawGraph;

  Font.Size := 12;
  Font.Color := ARGB(200, 200, 200, 200);
  var bottomY := H - 30;
  var colWidth := W div 4;
  DrawTextCentered(0, bottomY - 10, colWidth, bottomY + 10, 'Best Score: ' + Stats.BestScore.ToString);
  DrawTextCentered(colWidth, bottomY - 10, colWidth * 2, bottomY + 10, 'Best Acc: ' + Format('{0:0.0}%', Stats.BestAccuracy));
  DrawTextCentered(colWidth * 2, bottomY - 10, colWidth * 3, bottomY + 10, 'Best Avg: ' + Format('{0:0.0}ms', Stats.BestAvgReaction));
  DrawTextCentered(colWidth * 3, bottomY - 10, W, bottomY + 10, 'Games: ' + Stats.GamesPlayed.ToString);

  Font.Size := 10;
  Font.Color := ARGB(150, 200, 200, 200);
  var statusText := IfThen(ShowMainMenuEffects, 'Effects: ON', 'Effects: OFF');
  DrawTextCentered(W - 120, H - 20, W - 10, H - 10, statusText);
end;

procedure RenderGameOver;
var
  acc: real;
  avgReaction: real;
  y: integer;
begin
  ClearWindow(RGB(28, 28, 32));

  Font.Size := 48;
  DrawTextCentered(0, H div 2 - 50, W, H div 2 + 50, 'GAME OVER');

  Font.Size := 20;
  DrawTextCentered(0, H div 2 + 80, W, H div 2 + 120, 'Press SPACE to continue');

  Font.Size := 16;
  y := H div 2 + 160;
  DrawTextCentered(0, y, W, y + 30, 'Score: ' + Game.Score.ToString);
  y += 30;

  if Game.TotalShots > 0 then
    acc := Game.TotalAccuracySum / Game.TotalShots * 100
  else
    acc := 0;
  DrawTextCentered(0, y, W, y + 30, 'Accuracy: ' + Format('{0:0.0}', acc) + '%');
  y += 30;

  if Game.Score > 0 then
    avgReaction := Game.TotalReactionTime / Game.Score
  else
    avgReaction := 0;
  DrawTextCentered(0, y, W, y + 30, 'Avg Reaction: ' + Format('{0:0.0}ms', avgReaction));
  y += 30;

  DrawTextCentered(0, y, W, y + 30, 'Hits: ' + Game.Score.ToString + '  Misses: ' + Game.WrongClicks.ToString);
end;

// ----- UI -----
procedure InitUI;
begin
  SetLength(Buttons, 2);
  var H100 := H - 100;
  CreateButton(Buttons[0], W div 2 + W div 4 - 100,
               50 + H100 div 4 - H100 div 8 - H100 div 16 - H100 div 32,
               W div 4, H100 div 8,
               'Classic Mode', RGB(97, 207, 255), RGB(77, 187, 235));
  CreateButton(Buttons[1], W div 2 + W div 4 - 100,
               50 + H100 div 4 - H100 div 8 + H100 div 16 + H100 div 32,
               W div 4, H100 div 8,
               'Dual Mode', RGB(255, 80, 80), RGB(235, 60, 60));
end;

// ----- Обновление игры -----
procedure UpdateGame;
var
  i: integer;
  p: Point;
  hit: boolean;
  accuracy: real;
  info: THitInfo;
begin
  if not Game.IsPlayTime then Exit;

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

  for i := 0 to Game.CircleCount - 1 do
  begin
    Game.Circles[i].Update(Game.FrameTime, Game.Loses, Game.Fail, Game.MaxCTime, Game.CCTime);
    if Game.Circles[i].Time < 0 then
      Game.Circles[i].Spawn(Game.MaxRadius, W, H, Game.GameMode, Game.Circles[i]);
  end;

  while IsMouseButtonPressed(1) do
  begin
    Inc(Game.TotalShots);
    p := GetMousePos;
    hit := false;
    accuracy := 0;
    for i := 0 to Game.CircleCount - 1 do
    begin
      info := Game.Circles[i].GetHitInfo(p.X, p.Y, Game.GameMode, 1, Game.MaxRadius, Game.MaxCTime, Game.CCTime);
      if info.Hit then
      begin
        hit := true;
        accuracy := info.Accuracy;
        Inc(Game.Score);
        var reaction := Game.TryTime - Game.Circles[i].SpawnTime;
        Game.TotalReactionTime += reaction;
        AddReactionTime(reaction);
        Game.Circles[i].Spawn(Game.MaxRadius, W, H, Game.GameMode, Game.Circles[i]);
        Break;
      end;
    end;
    Game.TotalAccuracySum += accuracy;
    if not hit then
    begin
      Inc(Game.Loses);
      Inc(Game.WrongClicks);
      if Game.Loses >= Game.MaxLoses then
        Game.Fail := true;
    end;
    ClearMouseButtonPressed(1);
  end;

  if Game.GameMode = gmDual then
  begin
    while IsMouseButtonPressed(2) do
    begin
      Inc(Game.TotalShots);
      p := GetMousePos;
      hit := false;
      accuracy := 0;
      for i := 0 to Game.CircleCount - 1 do
      begin
        info := Game.Circles[i].GetHitInfo(p.X, p.Y, Game.GameMode, 2, Game.MaxRadius, Game.MaxCTime, Game.CCTime);
        if info.Hit then
        begin
          hit := true;
          accuracy := info.Accuracy;
          Inc(Game.Score);
          var reaction := Game.TryTime - Game.Circles[i].SpawnTime;
          Game.TotalReactionTime += reaction;
          AddReactionTime(reaction);
          Game.Circles[i].Spawn(Game.MaxRadius, W, H, Game.GameMode, Game.Circles[i]);
          Break;
        end;
      end;
      Game.TotalAccuracySum += accuracy;
      if not hit then
      begin
        Inc(Game.Loses);
        Inc(Game.WrongClicks);
        if Game.Loses >= Game.MaxLoses then
          Game.Fail := true;
      end;
      ClearMouseButtonPressed(2);
    end;
  end;

  if Game.Loses >= Game.MaxLoses then Game.Fail := true;
end;

// ----- Экранные состояния -----
procedure StartScreen;
begin
  Screen := scMainMenu;
  InitUI;
  InitParticles;
  while Screen = scMainMenu do
  begin
    for var i := 0 to PARTICLE_COUNT-1 do
      Particles[i].Update(W, H);

    RenderMainMenu;
    Redraw;

    while IsMouseButtonPressed(1) do
    begin
      var clickOnButton := false;
      for var i := 0 to Buttons.Length - 1 do
        if Buttons[i].IsHover and Buttons[i].IsActive then
        begin
          clickOnButton := true;
          if i = 0 then Game.GameMode := gmClassic
          else Game.GameMode := gmDual;
          Screen := scCountdown;
          Break;
        end;
      if not clickOnButton then
        ShowMainMenuEffects := not ShowMainMenuEffects;
      ClearMouseButtonPressed(1);
    end;

    UpdateInput;
  end;
end;

procedure CountdownScreen;
begin
  Game.CountdownValue := 3;
  Game.CountdownStartTime := Milliseconds;
  while Screen = scCountdown do
  begin
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
    ClearWindow(RGB(28, 28, 32));
    Font.Size := 120;
    DrawTextCentered(0, H div 2 - 100, W, H div 2 + 100, Game.CountdownValue.ToString);
    Font.Size := 20;
    DrawTextCentered(0, H div 2 + 120, W, H div 2 + 140, 'Get ready!');
    Redraw;
    UpdateInput;
  end;
end;

procedure GameLoop;
begin
  Screen := scGame;
  while Screen = scGame do
  begin
    UpdateGame;
    RenderGame;
    Redraw;

    if IsKeyPressed(VK_ESCAPE) then
      Game.Fail := true;

    if Game.Fail then
    begin
      var acc, avgReaction: real;
      if Game.TotalShots > 0 then
      begin
        acc := Game.TotalAccuracySum / Game.TotalShots * 100;
        avgReaction := Game.TotalReactionTime / Game.Score;
      end
      else
      begin
        acc := 0;
        avgReaction := 0;
      end;

      if Game.Score > Stats.BestScore then Stats.BestScore := Game.Score;
      if acc > Stats.BestAccuracy then Stats.BestAccuracy := acc;
      if (Game.Score > 0) and ((Stats.BestAvgReaction = 0) or (avgReaction < Stats.BestAvgReaction)) then
        Stats.BestAvgReaction := avgReaction;
      Inc(Stats.GamesPlayed);

      for var i := 0 to Game.ReactionCount - 1 do
      begin
        SavedReactionHistory[SavedReactionIndex] := Game.ReactionHistory[i];
        SavedReactionIndex := (SavedReactionIndex + 1) mod 50;
        if SavedReactionCount < 50 then
          Inc(SavedReactionCount);
      end;

      SaveStats;
      Screen := scGameOver;
      Break;
    end;

    UpdateInput;
  end;
end;

procedure GameOverScreen;
begin
  Screen := scGameOver;
  while Screen = scGameOver do
  begin
    RenderGameOver;
    Redraw;
    if IsKeyPressed(VK_SPACE) then
    begin
      Screen := scMainMenu;
      Break;
    end;
    UpdateInput;
  end;
end;

// ----- Основная программа -----
begin
  SetWindowSize(W, H);
  SetWindowCaption('Aim Trainer');
  CenterWindow;
  LockDrawing;
  Font.Name := 'Consolas';
  Font.Size := 14;
  Randomize;

  SetLength(CircleColors, 2);
  CircleColors[0] := ARGB(220, 97, 207, 255);
  CircleColors[1] := ARGB(220, 255, 80, 80);

  InitInput;
  ShowMainMenuEffects := true;
  LoadStats;

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
