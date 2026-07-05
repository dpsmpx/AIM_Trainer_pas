uses GraphABC, Control_AI, Base;

const
  ScreenMenu = 0;
  ScreenCountdown = 1;
  ScreenGame = 2;
  ScreenResults = 3;

  ModeClassic = 1;
  ModeDual = 2;

  WindowW = 900;
  WindowH = 650;
  HudHeight = 76;
  MaxReactionHistory = 50;
  GameDurationMs = 60000;
  MaxMisses = 15;
  TargetLifeMs = 1150;
  TargetGrowMs = 220;
  TargetFadeMs = 260;
  TargetMaxRadius = 38;
  TargetMinRadius = 8;

type
  TButton = record
    X, Y, Width, Height: integer;
    Text: string;
    NormalColor, HoverColor: Color;
    IsActive: boolean;

    function IsHover: boolean;
    begin
      Result := (MouseX >= X) and (MouseX <= X + Width) and
                (MouseY >= Y) and (MouseY <= Y + Height);
    end;

    function ContainsClick: boolean;
    begin
      if MouseJustPressed then
        Result := (MouseClickX >= X) and (MouseClickX <= X + Width) and
                  (MouseClickY >= Y) and (MouseClickY <= Y + Height)
      else
        Result := (MouseX >= X) and (MouseX <= X + Width) and
                  (MouseY >= Y) and (MouseY <= Y + Height);
    end;

    procedure Render;
    begin
      var c := ColorIf(IsActive and IsHover, HoverColor, NormalColor);
      var alpha := IntIf(IsActive and IsHover, 235, 180);
      DrawTextWithBackground(ARGB(alpha, GetRed(c), GetGreen(c), GetBlue(c)),
        X, Y, X + Width, Y + Height, Text, true);
    end;
  end;

  TTarget = record
    X, Y: integer;
    Radius: real;
    AgeMs: integer;
    SpawnTimeMs: integer;
    Kind: integer;
  end;

var
  Screen := ScreenMenu;
  GameMode := ModeClassic;
  Buttons: array of TButton;
  Target: TTarget;
  TargetColors: array of Color;
  ReactionHistory: array of integer;
  GraphPoints: array of Point;

  Score := 0;
  Shots := 0;
  Misses := 0;
  WrongClicks := 0;
  TotalReactionTime := 0;
  BestReaction := 0;
  WorstReaction := 0;

  FrameTime := 16;
  GameTimeMs := 0;
  CountdownMs := 0;
  LastFpsTick := 0;
  FpsFrames := 0;
  FPS := 0;
  HeldMouseConsumed := false;


function HasActionClick: boolean;
begin
  Result := MouseJustPressed or (MousePressed and not HeldMouseConsumed);
end;

function ActionClickX: integer;
begin
  if MouseJustPressed then
    Result := MouseClickX
  else
    Result := MouseX;
end;

function ActionClickY: integer;
begin
  if MouseJustPressed then
    Result := MouseClickY
  else
    Result := MouseY;
end;

function ActionClickButton: integer;
begin
  if MouseJustPressed and (MouseClickButton <> 0) then
    Result := MouseClickButton
  else if MouseCode <> 0 then
    Result := MouseCode
  else
    Result := LastMouseButton;
end;

procedure ConsumeActionClick;
begin
  ConsumeMousePress;
  if MousePressed then
    HeldMouseConsumed := true;
end;

procedure UpdateHeldMouseState;
begin
  if not MousePressed then
    HeldMouseConsumed := false;
end;


procedure SpawnTarget;
begin
  Target.X := Random(TargetMaxRadius + 10, WindowW - TargetMaxRadius - 10);
  Target.Y := Random(TargetMaxRadius + 10, WindowH - HudHeight - TargetMaxRadius - 10);
  Target.Radius := TargetMinRadius;
  Target.AgeMs := 0;
  Target.SpawnTimeMs := GameTimeMs;
  if GameMode = ModeDual then
    Target.Kind := Random(2)
  else
    Target.Kind := 0;
end;

procedure UpdateTarget(deltaMs: integer);
begin
  Target.AgeMs += deltaMs;
  if Target.AgeMs <= TargetGrowMs then
    Target.Radius := TargetMinRadius + (TargetMaxRadius - TargetMinRadius) * Target.AgeMs / TargetGrowMs
  else if Target.AgeMs >= TargetLifeMs - TargetFadeMs then
    Target.Radius := TargetMinRadius + (TargetMaxRadius - TargetMinRadius) *
      ClampReal((TargetLifeMs - Target.AgeMs) / TargetFadeMs, 0.0, 1.0)
  else
    Target.Radius := TargetMaxRadius;

  if Target.AgeMs >= TargetLifeMs then
  begin
    Misses += 1;
    SpawnTarget;
  end;
end;

procedure RenderTarget;
begin
  var c := TargetColors[Target.Kind];
  Brush.Color := c;
  Pen.Color := ARGB(230, 255, 255, 255);
  Pen.Width := 2;
  FillCircle(Target.X, Target.Y, Round(Target.Radius));
  Circle(Target.X, Target.Y, Round(Target.Radius));

  if GameMode = ModeDual then
  begin
    Font.Size := 12;
    Font.Color := clWhite;
    if Target.Kind = 0 then
      DrawTextCentered(Target.X - 18, Target.Y - 10, Target.X + 18, Target.Y + 10, 'LMB')
    else
      DrawTextCentered(Target.X - 18, Target.Y - 10, Target.X + 18, Target.Y + 10, 'RMB');
  end;
end;

procedure CreateButton(var b: TButton; x, y, w, h: integer; text: string; normalColor, hoverColor: Color);
begin
  b.X := x;
  b.Y := y;
  b.Width := w;
  b.Height := h;
  b.Text := text;
  b.NormalColor := normalColor;
  b.HoverColor := hoverColor;
  b.IsActive := true;
end;

procedure BuildMenuButtons;
begin
  SetLength(Buttons, 4);
  CreateButton(Buttons[0], WindowW div 2 - 180, 210, 360, 54, 'Classic mode', RGB(70, 170, 245), RGB(95, 195, 255));
  CreateButton(Buttons[1], WindowW div 2 - 180, 280, 360, 54, 'Dual mode', RGB(245, 85, 85), RGB(255, 115, 115));
  CreateButton(Buttons[2], WindowW div 2 - 180, 350, 360, 54, 'Restart last mode', RGB(115, 205, 110), RGB(145, 230, 140));
  CreateButton(Buttons[3], WindowW div 2 - 180, 420, 360, 54, 'Exit', RGB(110, 110, 120), RGB(145, 145, 155));
end;

function Accuracy: real;
begin
  Result := RealIf(Shots > 0, Score / Shots * 100.0, 0.0);
end;

function AvgReaction: real;
begin
  Result := RealIf(Score > 0, TotalReactionTime / Score, 0.0);
end;

procedure AddReaction(value: integer);
begin
  if value < 0 then value := 0;
  if Score = 1 then
  begin
    BestReaction := value;
    WorstReaction := value;
  end
  else
  begin
    if value < BestReaction then BestReaction := value;
    if value > WorstReaction then WorstReaction := value;
  end;

  if ReactionHistory.Length < MaxReactionHistory then
  begin
    SetLength(ReactionHistory, ReactionHistory.Length + 1);
    ReactionHistory[ReactionHistory.Length - 1] := value;
  end
  else
  begin
    for var i := 0 to ReactionHistory.Length - 2 do
      ReactionHistory[i] := ReactionHistory[i + 1];
    ReactionHistory[ReactionHistory.Length - 1] := value;
  end;
end;

procedure ResetGame;
begin
  Score := 0;
  Shots := 0;
  Misses := 0;
  WrongClicks := 0;
  TotalReactionTime := 0;
  BestReaction := 0;
  WorstReaction := 0;
  GameTimeMs := 0;
  CountdownMs := 2500;
  FPS := 0;
  FpsFrames := 0;
  LastFpsTick := 0;
  SetLength(ReactionHistory, 0);
  SpawnTarget;
  Screen := ScreenCountdown;
end;

procedure DrawHeader(title, subtitle: string);
begin
  Font.Size := 34;
  DrawTextWithBackground(ARGB(170, 55, 55, 65), 80, 48, WindowW - 80, 120, title, true);
  Font.Size := 14;
  Font.Color := ARGB(230, 235, 235, 245);
  DrawTextCentered(80, 130, WindowW - 80, 160, subtitle);
end;

procedure DrawHud;
begin
  var y1 := WindowH - HudHeight + 10;
  var y2 := WindowH - 12;
  var col := WindowW div 6;
  var bg := ARGB(210, 50, 50, 58);
  Font.Size := 13;
  DrawTextWithBackground(bg, 10, y1, col - 8, y2, 'FPS'#10 + FPS.ToString, true);
  DrawTextWithBackground(bg, col + 8, y1, 2 * col - 8, y2, 'Score'#10 + Score.ToString, true);
  DrawTextWithBackground(bg, 2 * col + 8, y1, 3 * col - 8, y2, 'Accuracy'#10 + FormatPercent(Accuracy), true);
  DrawTextWithBackground(bg, 3 * col + 8, y1, 4 * col - 8, y2, 'Avg'#10 + FormatMs(AvgReaction), true);
  DrawTextWithBackground(bg, 4 * col + 8, y1, 5 * col - 8, y2, 'Lives'#10 + IntToStr(MaxMisses - Misses), true);
  DrawTextWithBackground(bg, 5 * col + 8, y1, WindowW - 10, y2, 'Time'#10 + Format('{0:0.0}s', (GameDurationMs - GameTimeMs) / 1000.0), true);
end;

procedure DrawGraph(x, y, w, h: integer);
begin
  Brush.Color := ARGB(150, 34, 34, 42);
  Pen.Color := ARGB(70, 255, 255, 255);
  FillRoundRect(x, y, x + w, y + h, 12, 12);
  Font.Size := 12;
  Font.Color := ARGB(210, 235, 235, 245);
  DrawTextCentered(x, y + 6, x + w, y + 26, 'Last reaction times');

  if ReactionHistory.Length = 0 then
  begin
    Font.Color := ARGB(150, 235, 235, 245);
    DrawTextCentered(x, y + h div 2 - 12, x + w, y + h div 2 + 12, 'No hits yet');
    Exit;
  end;

  SetLength(GraphPoints, ReactionHistory.Length);
  var maxVal := 1;
  for var i := 0 to ReactionHistory.Length - 1 do
    if ReactionHistory[i] > maxVal then maxVal := ReactionHistory[i];

  var left := x + 24;
  var top := y + 34;
  var graphW := w - 48;
  var graphH := h - 70;
  for var i := 0 to ReactionHistory.Length - 1 do
  begin
    if ReactionHistory.Length = 1 then
      GraphPoints[i].X := left + graphW div 2
    else
      GraphPoints[i].X := left + Round(i * graphW / (ReactionHistory.Length - 1));
    GraphPoints[i].Y := top + graphH - Round(ReactionHistory[i] * graphH / maxVal);
  end;

  Pen.Color := ARGB(245, 97, 207, 255);
  Pen.Width := 3;
  if GraphPoints.Length = 1 then
    FillCircle(GraphPoints[0].X, GraphPoints[0].Y, 4)
  else
    Polyline(GraphPoints);

  Font.Size := 11;
  Font.Color := ARGB(220, 235, 235, 245);
  DrawTextCentered(x + 10, y + h - 30, x + w - 10, y + h - 8,
    'Best: ' + FormatMs(BestReaction) + '   Avg: ' + FormatMs(AvgReaction) + '   Worst: ' + FormatMs(WorstReaction));
end;

procedure RenderMenu;
begin
  ClearWindow(RGB(24, 24, 30));
  DrawHeader('AIM TRAINER', 'Left click blue targets. In Dual mode use right click for red targets. Esc/Space returns to menu.');
  for var i := 0 to Buttons.Length - 1 do
    Buttons[i].Render;
  DrawGraph(80, 500, WindowW - 160, 110);
end;

procedure ProcessMenu;
begin
  if KeyJustPressed then
    ConsumeKeyPress;

  if not HasActionClick then Exit;

  for var i := 0 to Buttons.Length - 1 do
    if Buttons[i].ContainsClick then
    begin
      ConsumeActionClick;
      case i of
        0: begin GameMode := ModeClassic; ResetGame; end;
        1: begin GameMode := ModeDual; ResetGame; end;
        2: ResetGame;
        3: Halt;
      end;
      Exit;
    end;

  ConsumeActionClick;
end;

procedure RenderCountdown;
begin
  ClearWindow(RGB(24, 24, 30));
  RenderTarget;
  DrawHud;
  Font.Size := 112;
  var n := CountdownMs div 1000 + 1;
  DrawTextWithBackground(ARGB(120, 255, 255, 255), WindowW div 2 - 110, WindowH div 2 - 100,
    WindowW div 2 + 110, WindowH div 2 + 100, n.ToString, true);
  Font.Size := 16;
  Font.Color := clWhite;
  DrawTextCentered(0, WindowH div 2 + 118, WindowW, WindowH div 2 + 148, 'Get ready');
end;

procedure UpdateCountdown;
begin
  if MouseJustPressed then
    ConsumeActionClick;

  if WasKeyPressed(27) or WasKeyPressed(VK_SPACE) then
  begin
    ConsumeKeyPress;
    Screen := ScreenMenu;
    Exit;
  end
  else if KeyJustPressed then
    ConsumeKeyPress;

  CountdownMs -= FrameTime;
  if CountdownMs <= 0 then
  begin
    GameTimeMs := 0;
    SpawnTarget;
    Screen := ScreenGame;
  end;
end;

procedure RegisterShot(hit: boolean; wrongButton: boolean);
begin
  Shots += 1;
  if hit then
  begin
    Score += 1;
    var reaction := GameTimeMs - Target.SpawnTimeMs;
    TotalReactionTime += reaction;
    AddReaction(reaction);
    SpawnTarget;
  end
  else
  begin
    Misses += 1;
    if wrongButton then WrongClicks += 1;
  end;
end;

procedure ProcessGameClick;
begin
  if not HasActionClick then Exit;

  var expectedButton := 1;
  if (GameMode = ModeDual) and (Target.Kind = 1) then
    expectedButton := 2;

  var clickButton := ActionClickButton;
  var correctButton := (GameMode = ModeClassic) or (clickButton = expectedButton);
  var inside := PointInCircle(ActionClickX, ActionClickY, Target.X, Target.Y, Target.Radius);
  RegisterShot(correctButton and inside, not correctButton);
  ConsumeActionClick;
end;

procedure UpdateGame;
begin
  if WasKeyPressed(27) or WasKeyPressed(VK_SPACE) then
  begin
    ConsumeKeyPress;
    Screen := ScreenMenu;
    Exit;
  end;

  GameTimeMs += FrameTime;
  UpdateTarget(FrameTime);
  ProcessGameClick;

  if (Misses >= MaxMisses) or (GameTimeMs >= GameDurationMs) then
    Screen := ScreenResults;
end;

procedure RenderGame;
begin
  ClearWindow(RGB(24, 24, 30));
  RenderTarget;
  DrawHud;
end;

procedure RenderResults;
begin
  ClearWindow(RGB(24, 24, 30));
  DrawHeader('RESULTS', 'R - restart, Space/Esc - menu');
  Font.Size := 18;
  DrawTextWithBackground(ARGB(180, 55, 55, 65), 160, 180, WindowW - 160, 330,
    'Score: ' + Score.ToString + #10 +
    'Shots: ' + Shots.ToString + #10 +
    'Misses: ' + Misses.ToString + '   Wrong buttons: ' + WrongClicks.ToString + #10 +
    'Accuracy: ' + FormatPercent(Accuracy) + #10 +
    'Average reaction: ' + FormatMs(AvgReaction), true);
  DrawGraph(120, 365, WindowW - 240, 190);
end;

procedure ProcessResults;
begin
  if WasKeyPressed(VK_R) then
  begin
    ConsumeKeyPress;
    ResetGame;
  end
  else if WasKeyPressed(VK_SPACE) or WasKeyPressed(27) then
  begin
    ConsumeKeyPress;
    Screen := ScreenMenu;
  end
  else if HasActionClick then
  begin
    ConsumeActionClick;
    Screen := ScreenMenu;
  end;
end;

procedure UpdateFrameTime;
begin
  FrameTime := Clamp(MillisecondsDelta, 1, 100);
  FpsFrames += 1;
  LastFpsTick += FrameTime;
  if LastFpsTick >= 1000 then
  begin
    FPS := Round(FpsFrames * 1000 / LastFpsTick);
    FpsFrames := 0;
    LastFpsTick := 0;
  end;
end;

procedure InitApp;
begin
  SetWindowSize(WindowW, WindowH);
  SetWindowCaption('Aim Trainer PascalABC.NET');
  CenterWindow;
  LockDrawing;
  InitControls;
  Randomize;
  Font.Name := 'Consolas';

  SetLength(TargetColors, 2);
  TargetColors[0] := ARGB(225, 97, 207, 255);
  TargetColors[1] := ARGB(225, 255, 85, 85);
  SetLength(ReactionHistory, 0);
  BuildMenuButtons;
end;

begin
  InitApp;
  while true do
  begin
    UpdateFrameTime;
    UpdateHeldMouseState;
    case Screen of
      ScreenMenu:
      begin
        ProcessMenu;
        RenderMenu;
      end;
      ScreenCountdown:
      begin
        UpdateCountdown;
        RenderCountdown;
      end;
      ScreenGame:
      begin
        UpdateGame;
        RenderGame;
      end;
      ScreenResults:
      begin
        ProcessResults;
        RenderResults;
      end;
    end;
    Redraw;
    FinishInputFrame;
  end;
end.
