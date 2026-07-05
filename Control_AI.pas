unit Control_AI;
Uses GraphABC;

var
  // Цветовые настройки
  BackColor, SelColor, MainColor: Color;
  
  // Состояния клавиш
  UP, DOWN, LEFT, RIGHT, ENTER, R, SPACE, W_, A, S, D: boolean;
  KeyPressed: boolean;
  KeyCode: integer;
  
  // Состояние мыши
  MouseX, MouseY: integer;
  MousePressed: boolean;
  MouseMoved: boolean;
  MouseCode: integer; // 1-ЛКМ, 2-ПКМ, 3-СКМ
  
  // Системные флаги
  Resized: boolean;
  Pause: integer;
  ActiveEdit: integer;

// Обработчик нажатия клавиш
procedure KeyDown(key: integer);
begin
  case key of
    VK_UP: UP := true;
    VK_DOWN: DOWN := true;
    VK_LEFT: LEFT := true;
    VK_RIGHT: RIGHT := true;
    VK_ENTER: ENTER := true;
    VK_R: R := true;
    VK_SPACE: SPACE := true;
    VK_W: W_ := true;
    VK_S: S := true;
    VK_A: A := true;
    VK_D: D := true;
  end;
  KeyPressed := true;
  KeyCode := key;
end;

// Обработчик отпускания клавиш
procedure KeyUp(key: integer);
begin
  case key of
    VK_UP: UP := false;
    VK_DOWN: DOWN := false;
    VK_LEFT: LEFT := false;
    VK_RIGHT: RIGHT := false;
    VK_ENTER: ENTER := false;
    VK_R: R := false;
    VK_SPACE: SPACE := false;
    VK_W: W_ := false;
    VK_S: S := false;
    VK_A: A := false;
    VK_D: D := false;
  end;
  KeyPressed := false;
  KeyCode := -1;
end;

function IsKeyPressed(key: integer): boolean;
begin
  result := KeyPressed and (KeyCode = key);
end;

// Обработчик нажатия кнопок мыши
procedure MouseDown(x, y, mb: integer);
begin
  MouseX := x;
  MouseY := y;
  MousePressed := true;
  MouseCode := mb; // Сохраняем код кнопки
end;

// Обработчик движения мыши
procedure MouseMove(x, y, mb: integer);
begin
  MouseX := x;
  MouseY := y;
  MouseCode := mb;
  MouseMoved := true;
end;

// Обработчик отпускания кнопок мыши
procedure MouseUp(x, y, mb: integer);
begin
  MousePressed := false;
  MouseCode := 0; // Сброс кода кнопки
end;

// Обработчик изменения размера окна
procedure Resize;
begin
  Resized := true;
end;

// Инициализация обработчиков событий
begin
  OnKeyDown := KeyDown;
  OnKeyUp := KeyUp;
  OnMouseDown := MouseDown;
  OnMouseMove := MouseMove;
  OnMouseUp := MouseUp;
  OnResize := Resize;
  
  // Начальные значения
  MouseCode := 0;
  KeyCode := -1;
  ActiveEdit := -1;
end.
