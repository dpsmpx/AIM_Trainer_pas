unit Base;

interface

uses GraphABC;

/// Универсальный тернарный оператор (работает с любым типом)
function IfThen<T>(Condition: Boolean; TrueValue, FalseValue: T): T;

// ===== Процедуры работы с графикой =====
procedure DrawTextWithBackground(bgColor: Color; x1, y1, x2, y2, fontSize: integer; text: string; shadow: boolean);
procedure DrawTextWithBackground(bgColor: Color; x1, y1, x2, y2, fontSize: integer; number: integer; shadow: boolean);
procedure DrawTextWithBackground(bgColor: Color; x1, y1, x2, y2, fontSize: integer; value: real; shadow: boolean);

/// Рисует кубическую кривую Безье (значение по умолчанию только в интерфейсе)
procedure DrawBezierLine(x1, y1, cx1, cy1, cx2, cy2, x2, y2: integer; color: Color; width: integer := 1);

implementation

function IfThen<T>(Condition: Boolean; TrueValue, FalseValue: T): T;
begin
  if Condition then Result := TrueValue
               else Result := FalseValue;
end;

// В реализации значение по умолчанию НЕ УКАЗЫВАЕМ (оно уже есть в интерфейсе)
procedure DrawBezierLine(x1, y1, cx1, cy1, cx2, cy2, x2, y2: integer; color: Color; width: integer);
var
  t, step: double;
  points: array of Point;
  i: integer;
  t_sqr, t_cub, nt, nt_sqr, nt_cub: double;
begin
  step := 0.02;
  SetLength(points, Round(1 / step) + 1);
  
  Pen.Color := color;
  Pen.Width := width;
  
  t := 0.0;
  for i := 0 to High(points) do
  begin
    t_sqr := t * t;
    t_cub := t_sqr * t;
    nt := 1 - t;
    nt_sqr := nt * nt;
    nt_cub := nt_sqr * nt;
    
    points[i].X := Round(
      nt_cub * x1 +
      3 * nt_sqr * t * cx1 +
      3 * nt * t_sqr * cx2 +
      t_cub * x2
    );
    points[i].Y := Round(
      nt_cub * y1 +
      3 * nt_sqr * t * cy1 +
      3 * nt * t_sqr * cy2 +
      t_cub * y2
    );
    
    t += step;
  end;
  
  Polyline(points);
end;

procedure DrawTextWithBackground(bgColor: Color; x1, y1, x2, y2, fontSize: integer; text: string; shadow: boolean);
begin
  Brush.Color := bgColor;
  FillRoundRect(x1, y1, x2, y2, 16, 16);
  SetFontSize(fontSize);
  
  if shadow then
  begin
    Font.Color := ARGB(120, 0, 0, 0);
    DrawTextCentered(x1 + 2, y1 + 2, x2 + 2, y2 + 2, text);
  end;
  
  Font.Color := clWhite;
  DrawTextCentered(x1, y1, x2, y2, text);
end;

procedure DrawTextWithBackground(bgColor: Color; x1, y1, x2, y2, fontSize: integer; number: integer; shadow: boolean);
begin
  DrawTextWithBackground(bgColor, x1, y1, x2, y2, fontSize, IntToStr(number), shadow);
end;

procedure DrawTextWithBackground(bgColor: Color; x1, y1, x2, y2, fontSize: integer; value: real; shadow: boolean);
begin
  DrawTextWithBackground(bgColor, x1, y1, x2, y2, fontSize, Format('{0:0.0}', value), shadow);
end;

end.
