unit Control_AI;

interface

uses GraphABC;

type
  TButtonState = record
    Held: boolean;
    Pressed: boolean;
    Released: boolean;
  end;

  TInputState = record
    Keys: array[0..255] of TButtonState;
    MouseX, MouseY: integer;
    MouseButtons: array[1..3] of TButtonState;
    MouseMoved: boolean;
    Resized: boolean;
  end;

var
  Input: TInputState;

procedure InitInput;
procedure UpdateInput;

function IsKeyHeld(key: integer): boolean;
function IsKeyPressed(key: integer): boolean;
function IsKeyReleased(key: integer): boolean;

function IsMouseButtonHeld(button: integer): boolean;
function IsMouseButtonPressed(button: integer): boolean;
function IsMouseButtonReleased(button: integer): boolean;

// Сбросить флаг Pressed для указанной кнопки мыши (после обработки события)
procedure ClearMouseButtonPressed(button: integer);

function GetMousePos: Point;
function IsResized: boolean;

const
  KEY_UP    = VK_UP;
  KEY_DOWN  = VK_DOWN;
  KEY_LEFT  = VK_LEFT;
  KEY_RIGHT = VK_RIGHT;
  KEY_ENTER = VK_ENTER;
  KEY_R     = VK_R;
  KEY_SPACE = VK_SPACE;
  KEY_W     = VK_W;
  KEY_A     = VK_A;
  KEY_S     = VK_S;
  KEY_D     = VK_D;

implementation

procedure KeyDown(key: integer);
begin
  if (key >= 0) and (key <= 255) then
  begin
    Input.Keys[key].Held := true;
    Input.Keys[key].Pressed := true;
  end;
end;

procedure KeyUp(key: integer);
begin
  if (key >= 0) and (key <= 255) then
  begin
    Input.Keys[key].Held := false;
    Input.Keys[key].Released := true;
  end;
end;

procedure MouseDown(x, y, mb: integer);
begin
  Input.MouseX := x;
  Input.MouseY := y;
  if (mb >= 1) and (mb <= 3) then
  begin
    Input.MouseButtons[mb].Held := true;
    Input.MouseButtons[mb].Pressed := true;
  end;
end;

procedure MouseUp(x, y, mb: integer);
begin
  Input.MouseX := x;
  Input.MouseY := y;
  if (mb >= 1) and (mb <= 3) then
  begin
    Input.MouseButtons[mb].Held := false;
    Input.MouseButtons[mb].Released := true;
  end;
end;

procedure MouseMove(x, y, mb: integer);
begin
  Input.MouseX := x;
  Input.MouseY := y;
  Input.MouseMoved := true;
end;

procedure Resize;
begin
  Input.Resized := true;
end;

procedure InitInput;
var
  i: integer;
begin
  for i := 0 to 255 do
  begin
    Input.Keys[i].Held := false;
    Input.Keys[i].Pressed := false;
    Input.Keys[i].Released := false;
  end;
  for i := 1 to 3 do
  begin
    Input.MouseButtons[i].Held := false;
    Input.MouseButtons[i].Pressed := false;
    Input.MouseButtons[i].Released := false;
  end;
  Input.MouseX := 0;
  Input.MouseY := 0;
  Input.MouseMoved := false;
  Input.Resized := false;

  OnKeyDown := KeyDown;
  OnKeyUp := KeyUp;
  OnMouseDown := MouseDown;
  OnMouseUp := MouseUp;
  OnMouseMove := MouseMove;
  OnResize := Resize;
end;

procedure UpdateInput;
var
  i: integer;
begin
  // Сбрасываем Pressed и Released для клавиш (для клавиш оставляем как есть)
  for i := 0 to 255 do
  begin
    Input.Keys[i].Pressed := false;
    Input.Keys[i].Released := false;
  end;
  // Для мыши сбрасываем только Released, Pressed сбрасываем вручную через ClearMouseButtonPressed
  for i := 1 to 3 do
    Input.MouseButtons[i].Released := false;
  Input.MouseMoved := false;
  Input.Resized := false;
end;

procedure ClearMouseButtonPressed(button: integer);
begin
  if (button >= 1) and (button <= 3) then
    Input.MouseButtons[button].Pressed := false;
end;

function IsKeyHeld(key: integer): boolean;
begin
  if (key >= 0) and (key <= 255) then
    Result := Input.Keys[key].Held
  else
    Result := false;
end;

function IsKeyPressed(key: integer): boolean;
begin
  if (key >= 0) and (key <= 255) then
    Result := Input.Keys[key].Pressed
  else
    Result := false;
end;

function IsKeyReleased(key: integer): boolean;
begin
  if (key >= 0) and (key <= 255) then
    Result := Input.Keys[key].Released
  else
    Result := false;
end;

function IsMouseButtonHeld(button: integer): boolean;
begin
  if (button >= 1) and (button <= 3) then
    Result := Input.MouseButtons[button].Held
  else
    Result := false;
end;

function IsMouseButtonPressed(button: integer): boolean;
begin
  if (button >= 1) and (button <= 3) then
    Result := Input.MouseButtons[button].Pressed
  else
    Result := false;
end;

function IsMouseButtonReleased(button: integer): boolean;
begin
  if (button >= 1) and (button <= 3) then
    Result := Input.MouseButtons[button].Released
  else
    Result := false;
end;

function GetMousePos: Point;
begin
  Result.X := Input.MouseX;
  Result.Y := Input.MouseY;
end;

function IsResized: boolean;
begin
  Result := Input.Resized;
end;

end.
