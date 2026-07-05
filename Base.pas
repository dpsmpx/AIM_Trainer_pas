unit Base;

interface

uses GraphABC;

// Базовые математические функции
function Len2D(x1, y1, x2, y2: double): double;
function IfThen(Condition: Boolean; TrueValue, FalseValue: Color): Color;
function IfThen(Condition: Boolean; TrueValue, FalseValue: Integer): Integer;
function IfThen(Condition: Boolean; TrueValue, FalseValue: String): String;
function IfThen(Condition: Boolean; TrueValue, FalseValue: Real): Real;

// Процедуры работы с графикой
procedure DrawTextWithBackground(bgColor: Color; x1, y1, x2, y2: integer; text: string; shadow: boolean);
procedure DrawTextWithBackground(bgColor: Color; x1, y1, x2, y2: integer; number: integer; shadow: boolean);
procedure DrawTextWithBackground(bgColor: Color; x1, y1, x2, y2: integer; value: real; shadow: boolean);

implementation

function Len2D(x1, y1, x2, y2: double): double;
begin
  Result := Sqrt(Sqr(x2 - x1) + Sqr(y2 - y1));
end;

function IfThen(Condition: Boolean; TrueValue, FalseValue: Color): Color;
begin
  Result := Condition ? TrueValue : FalseValue;
end;

function IfThen(Condition: Boolean; TrueValue, FalseValue: Integer): Integer;
begin
  Result := Condition ? TrueValue : FalseValue;
end;

function IfThen(Condition: Boolean; TrueValue, FalseValue: String): String;
begin
  Result := Condition ? TrueValue : FalseValue;
end;

function IfThen(Condition: Boolean; TrueValue, FalseValue: Real): Real;
begin
  Result := Condition ? TrueValue : FalseValue;
end;

procedure DrawBezierLine(x1, y1, cx1, cy1, cx2, cy2, x2, y2: integer; color: Color; width: integer := 1);
begin
  var t := 0.0;
  var step := 0.02;
  var points := new Point[Round(1/step)+1];
  
  Pen.Color := color;
  Pen.Width := width;
  
  for var i := 0 to points.Length-1 do
  begin
    var t_sqr := t*t;
    var t_cub := t_sqr*t;
    var nt := 1 - t;
    var nt_sqr := nt*nt;
    var nt_cub := nt_sqr*nt;
    
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

procedure DrawTextWithBackground(bgColor: Color; x1, y1, x2, y2: integer; text: string; shadow: boolean);
begin
  // Рисуем фон
  Brush.Color := bgColor;
  FillRoundRect(x1, y1, x2, y2, 16, 16);
  
  // Рисуем тень если нужно
  if shadow then
  begin
    Font.Color := ARGB(120, 0, 0, 0);
    DrawTextCentered(x1 + 2, y1 + 2, x2 + 2, y2 + 2, text);
  end;
  
  // Основной текст
  Font.Color := clWhite;
  DrawTextCentered(x1, y1, x2, y2, text);
end;

procedure DrawTextWithBackground(bgColor: Color; x1, y1, x2, y2: integer; number: integer; shadow: boolean);
begin
  DrawTextWithBackground(bgColor, x1, y1, x2, y2, IntToStr(number), shadow);
end;

procedure DrawTextWithBackground(bgColor: Color; x1, y1, x2, y2: integer; value: real; shadow: boolean);
begin
  DrawTextWithBackground(bgColor, x1, y1, x2, y2, Format('{0:0.0}', value), shadow);
end;

end.
