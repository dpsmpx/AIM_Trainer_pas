uses GraphABC, Control_AI, Base;

type
  TButton = record
    X, Y, Width, Height: integer;
    Text: string;
    NormalColor, HoverColor: Color;
    CurrentAlpha: integer;
    IsActive: boolean;
    
    procedure Render(shadow: boolean);
    begin
      var useColor := IfThen(IsHover and IsActive, HoverColor, NormalColor);
      DrawTextWithBackground(
        ARGB(CurrentAlpha, GetRed(useColor), GetGreen(useColor), GetBlue(useColor)),
        X, Y, X + Width, Y + Height,
        Text,
        shadow);
    end;
    
    function IsHover: boolean;
    begin
      Result := (MouseX >= X) and (MouseX <= X + Width) and
                (MouseY >= Y) and (MouseY <= Y + Height);
    end;
  end;

var
  W := 800;
  H := 600;
  MaxRadius := 40;
  Circle_Count := 1;
  MaxCTime := 1000;
  CCTime := MaxCTime div 2;
  CircleTouched := False;
  LosesAdded := False;
  
  // Статистика
  Score := 0;
  TotalShots := 0;
  Loses := 0;
  WrongClicks := 0;
  TotalReactionTime := 0;
  Max_Loses := 15;
  Fail := false;
  
  // Тайминг
  FrameTime := 0;
  LastTime := 0;
  FPS := 0;
  tempFPS := 0;
  TryTime := 0;
  MaxTryTime := 1000 * 60;
  IsPlayTime: boolean;
  
  // Режимы и графика
  GameMode := 0;
  ReactionHistory: array of integer;
  GraphPoints: array of Point;
  CircleColors: array of Color;
  
  // Управление и кнопки
  Buttons: array of TButton;
  SavedGameMode: integer;
  Screen: integer;
  prevMousePressed: boolean;

type
  Circle = record
    X, Y, Radius: integer;
    Time, SpawnTime: integer;
    CType: integer;
    
    procedure Spawn;
    begin
      X := Random(MaxRadius, W - MaxRadius);
      Y := Random(MaxRadius, H - MaxRadius - 60);
      Time := MaxCTime;
      SpawnTime := TryTime;
      if GameMode = 2 then CType := Random(0, 1);
    end;
    
    procedure Render;
    begin
      if not IsPlayTime then Exit;
      if Time < MaxCTime then
      begin
        if Time > CCTime then
          Radius := MaxRadius - Round(MaxRadius * ((Time - CCTime) / CCTime))
        else
          Radius := Round(MaxRadius * (Time / CCTime));
        
        Brush.Color := CircleColors[CType];
        FillCircle(X, Y, Radius);
      end;
      
      Time -= FrameTime;
      if Time < 0 then
      begin
        Loses += 1;
        Spawn;
      end;
    end;
    
    procedure Touch;
    begin
      if not IsPlayTime then Exit;
      
      var localMousePressed: boolean;
      if MousePressed and not localMousePressed then
      begin
        localMousePressed := true;
        var validClick := false;
        
        case GameMode of
          1: validClick := (MouseCode = 1);
          2: validClick := ((CType = 0) and (MouseCode = 1)) or 
                           ((CType = 1) and (MouseCode = 2));
        end;
        
        if validClick and (Time < MaxCTime) and 
           (Len2D(MouseX, MouseY, X, Y) < Radius) then
        begin
          localMousePressed := False;
          CircleTouched := True;
          TotalShots += 1;
          Score += 1;
          var reaction := TryTime - SpawnTime;
          TotalReactionTime += reaction;
          
          SetLength(ReactionHistory, ReactionHistory.Length + 1);
          ReactionHistory[ReactionHistory.Length - 1] := reaction;
          if ReactionHistory.Length > 50 then 
            SetLength(ReactionHistory, 50);

          Spawn;
        end
        else if not validClick then
        begin
          localMousePressed := False;
          CircleTouched := False;
          TotalShots += 1;
          Loses += 1;
          WrongClicks += 1;
        end;
      end
      else if not MousePressed then
      begin
        localMousePressed := false;
      end;
    end;
  end;

var
  Circles: array of Circle;

procedure CreateButton(var btn: TButton; x, y, w, h: integer; text: string; normalColor, hoverColor: Color);
begin
  btn.X := x;
  btn.Y := y;
  btn.Width := w;
  btn.Height := h;
  btn.Text := text;
  btn.NormalColor := normalColor;
  btn.HoverColor := hoverColor;
  btn.CurrentAlpha := 150;
end;

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

procedure InitApp;
begin
  SetWindowSize(W, H);
  SetWindowCaption('Aim Trainer');
  CenterWindow;
  LockDrawing;
  Font.Name := 'Consolas';
  Font.Size := 14;
  
  SetLength(CircleColors, 2);
  CircleColors[0] := ARGB(220, 97, 207, 255);
  CircleColors[1] := ARGB(220, 255, 80, 80);
  
  SetLength(Circles, Circle_Count);
  SetLength(ReactionHistory, 0);
  SetLength(Buttons, 0);
  
  InitUI;
end;

procedure InitGame;
begin
  CircleTouched := False;
  LosesAdded := False;
  Score := 0;
  TotalShots := 0;
  Loses := 0;
  WrongClicks := 0;
  TotalReactionTime := 0;
  LastTime := 1000;
  IsPlayTime := true;  // Включаем игровой процесс
  Circles[0].Spawn;
  Fail := False;       // Сбрасываем флаг поражения
end;

procedure RenderUpdateCircles;
begin
  for var i := 0 to Circles.Length - 1 do
  begin
    Circles[i].Render;
    Circles[i].Touch;
  end;
end;

procedure Render;
begin
  ClearWindow(RGB(28, 28, 32));
  if IsPlayTime then
    RenderUpdateCircles;
  
  // Сохраняем настройки шрифта
  var oldFontSize := Font.Size;
  Font.Size := 14;
  
  // Информационная панель
  var bgColor := ARGB(220, 60, 60, 65);
  var infoH := 60;
  var colWidth := W div 5;
  var paddingW := 16;
  var bottom := 16;
  
  DrawTextWithBackground(bgColor, 0 + paddingW,
                         H - infoH, colWidth - paddingW,
                         H - bottom, 'FPS:'#10 + FPS.ToString, True);
  
  // Отрисовка элементов
  DrawTextWithBackground(bgColor, colWidth + paddingW,
                         H - infoH, 2 * colWidth - paddingW,
                         H - bottom, 'Score:'#10 + Score, True);
  
  // Accuracy
  var accuracy := IfThen(TotalShots > 0, (Score / TotalShots * 100), 0.0);
  DrawTextWithBackground(bgColor, 2 * colWidth + paddingW,
                         H - infoH, 3 * colWidth - paddingW,
                         H - bottom, 'Accuracy:'#10 + Format('{0:0.0}%', accuracy), True);
  
  // Avg Reaction
  var avgReaction := IfThen(Score > 0, TotalReactionTime / Score, 0.0);
  DrawTextWithBackground(bgColor, 3 * colWidth + paddingW,
                         H - infoH, 4 * colWidth - paddingW,
                         H - bottom, 'Avg:'#10 + Format('{0:0.0}ms', avgReaction), True);
  
  // Lives
  DrawTextWithBackground(bgColor, 4 * colWidth + paddingW,
                         H - infoH, W - paddingW,
                         H - bottom, 'Lives:'#10 + (Max_Loses - Loses), True);
  
  Font.Size := oldFontSize;
end;

procedure Countdown;
begin
  IsPlayTime := false;
  var oldFontSize := Font.Size;
  var i := 2;
  
  while i > 0 do
  begin
    Render;
    Brush.Color := ARGB(32, 255, 255, 255);
    FillCircle(W div 2, H div 2, 100);
    Font.Size := 120;
    DrawTextCentered(0, H div 2 - 100, W, H div 2 + 100, IntToStr(i));
    Font.Size := 20;
    DrawTextCentered(0, H div 2 + 120, W, H div 2 + 140, 'Start on 0!');
    Redraw;
    Sleep(500);
    i -= 1;
  end;
  
  Font.Size := oldFontSize;
  IsPlayTime := true;
end;

procedure Update;
begin
  // Обработка пробела для возврата в меню
  if IsKeyPressed(VK_Space) then
  begin
    Screen := 0;
    Fail := True;
    while IsKeyPressed(VK_Space) do;
  end;
  
  if MousePressed and not prevMousePressed then
  begin
    TotalShots += 1;
    if ((GameMode = 2) and not CircleTouched) or 
       (not CircleTouched and not LosesAdded) then
    begin
      Loses += 1;
      LosesAdded := True;
    end;
    prevMousePressed := MousePressed;
  end;
  
  if not MousePressed then
  begin
    CircleTouched := False;
    LosesAdded := False;
  end;
  
  if Loses >= Max_Loses then 
  begin
    Fail := True;
    IsPlayTime := false; // Дополнительная защита
  end;
  
  // FPS
  FrameTime := MillisecondsDelta;
  TryTime += FrameTime;
  tempFPS += 1;
  
  if LastTime > 0 then 
    LastTime -= FrameTime 
  else
  begin
    FPS := tempFPS;
    tempFPS := 0;
    LastTime := 1000;
  end;
end;

procedure GamePlay;
begin
  IsPlayTime := true; // Включаем игровой процесс только здесь
  InitGame;       // Сброс состояния
  Fail := False;  // Явный сброс флага завершения
  Countdown;      // Запуск отсчёта
  
  while not Fail do
  begin
    Render;
    Redraw;
    Update;
  end;
  
  IsPlayTime := false; // Выключаем при завершении
  Screen := 0;
end;

procedure StartScreen;
begin
  InitUI;
  IsPlayTime := false;
  // Активируем кнопки
  for var i := 0 to Buttons.Length - 1 do
    Buttons[i].IsActive := true;

  while Screen = 0 do
  begin
    Render;
    ClearWindow(ARGB(32, 255, 255, 255));
    
    // Заголовок
    DrawTextWithBackground(ARGB(150, 100, 100, 100), 50, 50, W div 2, H div 2 - 50, 'AIM TRAINER', True);
    
    // Кнопки
    for var i := 0 to Buttons.Length - 1 do
    begin
      Buttons[i].CurrentAlpha := IfThen(Buttons[i].IsHover, 220, 150);
      Buttons[i].Render(True);
    end;
    
    // График в нижней половине
    var GraphY := H div 2;
    var GraphH := H div 2 - 110;
    Brush.Color := ARGB(150, 100, 100, 100);
    FillRoundRect(50, GraphY, W - 50, GraphY + GraphH, 8, 8);
    
    if ReactionHistory.Length > 0 then
    begin
      SetLength(GraphPoints, ReactionHistory.Length);
      var maxVal := ReactionHistory.Max();
      if maxVal = 0 then maxVal := 1;
      
      for var i := 0 to ReactionHistory.Length - 1 do
      begin
        GraphPoints[i].X := 50 + Round(i * ((W - 100) / ReactionHistory.Length));
        GraphPoints[i].Y := GraphY + GraphH - Round(ReactionHistory[i] * (GraphH / maxVal));
      end;
      
      Pen.Color := ARGB(255, 97, 207, 255);
      Pen.Width := 2;
      Polyline(GraphPoints);
    end;
    
    Redraw;
    
    // Обработка клавиши пробела
    if IsKeyPressed(VK_Space) then
    begin
      Screen := 0;
      while IsKeyPressed(VK_Space) do;
    end;
    
    // Обработка клика по кнопкам
    if MousePressed then
    begin
      for var i := 0 to Buttons.Length - 1 do
      begin
        if Buttons[i].IsHover and Buttons[i].IsActive then
        begin
          GameMode := i + 1;
          Screen := 1;  // Явно устанавливаем экран игры
          IsPlayTime := true;
          //GamePlay;
          Break;
        end;
      end;
      while MousePressed do; // Ждём отпускания кнопки
    end;
  end;
  
  IsPlayTime := true;
end;

procedure DrawGraph;
var
  GraphX := 50;
  GraphY := H div 2 - 50;
  GraphW := W - 100;
  GraphH := H div 2 - 100;
begin
  Brush.Color := ARGB(150, 30, 30, 35);
  FillRoundRect(GraphX, GraphY, GraphX + GraphW, GraphY + GraphH, 8, 8);
  
  if ReactionHistory.Length > 0 then
  begin
    SetLength(GraphPoints, ReactionHistory.Length);
    var maxVal := ReactionHistory.Max();
    if maxVal = 0 then maxVal := 1;
    
    for var i := 0 to ReactionHistory.Length - 1 do
    begin
      GraphPoints[i].X := GraphX + Round(i * (GraphW / ReactionHistory.Length));
      GraphPoints[i].Y := GraphY + GraphH - Round(ReactionHistory[i] * (GraphH / maxVal));
    end;
    
    Pen.Color := ARGB(255, 97, 207, 255);
    Pen.Width := 2;
    Polyline(GraphPoints);
  end;
end;

begin
  InitApp;
  while True do
  begin
    if Screen = 0 then
      StartScreen
    else if Screen = 1 then
      GamePlay;
  end;
end.
