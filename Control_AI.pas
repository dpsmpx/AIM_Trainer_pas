unit Control_AI;

interface

uses GraphABC;

var
  MouseX, MouseY: integer;
  MousePressed: boolean;
  MouseJustPressed: boolean;
  MouseJustReleased: boolean;
  MouseMoved: boolean;
  MouseCode: integer;
  LastMouseButton: integer;
  MouseClickX, MouseClickY: integer;
  MouseClickButton: integer;

  KeyPressed: boolean;
  KeyCode: integer;
  LastKeyCode: integer;
  KeyJustPressed: boolean;

  UP, DOWN, LEFT, RIGHT, ENTER, R, SPACE, ESCAPE, W_, A, S, D: boolean;
  Resized: boolean;

function IsKeyPressed(key: integer): boolean;
function WasKeyPressed(key: integer): boolean;
procedure ConsumeMousePress;
procedure ConsumeKeyPress;
procedure FinishInputFrame;

implementation

procedure SetKeyState(key: integer; state: boolean);
begin
  case key of
    VK_UP: UP := state;
    VK_DOWN: DOWN := state;
    VK_LEFT: LEFT := state;
    VK_RIGHT: RIGHT := state;
    VK_ENTER: ENTER := state;
    VK_R: R := state;
    VK_SPACE: SPACE := state;
    27: ESCAPE := state;
    VK_W: W_ := state;
    VK_A: A := state;
    VK_S: S := state;
    VK_D: D := state;
  end;
end;

procedure KeyDown(key: integer);
begin
  if not ((KeyPressed) and (KeyCode = key)) then
  begin
    KeyJustPressed := true;
    LastKeyCode := key;
  end;
  KeyPressed := true;
  KeyCode := key;
  SetKeyState(key, true);
end;

procedure KeyUp(key: integer);
begin
  SetKeyState(key, false);
  if KeyCode = key then
  begin
    KeyPressed := false;
    KeyCode := -1;
  end;
end;

function IsKeyPressed(key: integer): boolean;
begin
  Result := KeyPressed and (KeyCode = key);
end;

function WasKeyPressed(key: integer): boolean;
begin
  Result := KeyJustPressed and (LastKeyCode = key);
end;

procedure MouseDown(x, y, mb: integer);
begin
  MouseX := x;
  MouseY := y;
  MouseJustPressed := true;
  MouseClickX := x;
  MouseClickY := y;
  MouseClickButton := mb;
  MousePressed := true;
  MouseCode := mb;
  LastMouseButton := mb;
end;

procedure MouseMove(x, y, mb: integer);
begin
  MouseX := x;
  MouseY := y;
  if mb <> 0 then
    MouseCode := mb;
  MouseMoved := true;
end;

procedure MouseUp(x, y, mb: integer);
begin
  MouseX := x;
  MouseY := y;
  MousePressed := false;
  MouseJustReleased := true;
  MouseCode := 0;
end;

procedure Resize;
begin
  Resized := true;
end;

procedure ConsumeMousePress;
begin
  MouseJustPressed := false;
  MouseClickButton := 0;
end;

procedure ConsumeKeyPress;
begin
  KeyJustPressed := false;
  LastKeyCode := -1;
end;

procedure FinishInputFrame;
begin
  MouseJustReleased := false;
  MouseMoved := false;
  Resized := false;
end;

begin
  OnKeyDown := KeyDown;
  OnKeyUp := KeyUp;
  OnMouseDown := MouseDown;
  OnMouseMove := MouseMove;
  OnMouseUp := MouseUp;
  OnResize := Resize;

  KeyCode := -1;
  LastKeyCode := -1;
  MouseCode := 0;
  LastMouseButton := 0;
  MouseClickButton := 0;
end.
