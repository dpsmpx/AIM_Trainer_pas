unit Base;

interface

uses GraphABC;

function DistanceSquared(x1, y1, x2, y2: real): real;
function PointInCircle(px, py, cx, cy, radius: real): boolean;
function Clamp(value, minValue, maxValue: integer): integer;
function ClampReal(value, minValue, maxValue: real): real;
function ColorIf(condition: boolean; trueValue, falseValue: Color): Color;
function IntIf(condition: boolean; trueValue, falseValue: integer): integer;
function RealIf(condition: boolean; trueValue, falseValue: real): real;
function FormatMs(value: real): string;
function FormatPercent(value: real): string;

procedure DrawTextWithBackground(bgColor: Color; x1, y1, x2, y2: integer; text: string; shadow: boolean);
procedure DrawTextWithBackground(bgColor: Color; x1, y1, x2, y2: integer; number: integer; shadow: boolean);
procedure DrawTextWithBackground(bgColor: Color; x1, y1, x2, y2: integer; value: real; shadow: boolean);

implementation

function DistanceSquared(x1, y1, x2, y2: real): real;
begin
  Result := Sqr(x2 - x1) + Sqr(y2 - y1);
end;

function PointInCircle(px, py, cx, cy, radius: real): boolean;
begin
  Result := DistanceSquared(px, py, cx, cy) <= Sqr(radius);
end;

function Clamp(value, minValue, maxValue: integer): integer;
begin
  if value < minValue then
    Result := minValue
  else if value > maxValue then
    Result := maxValue
  else
    Result := value;
end;

function ClampReal(value, minValue, maxValue: real): real;
begin
  if value < minValue then
    Result := minValue
  else if value > maxValue then
    Result := maxValue
  else
    Result := value;
end;

function ColorIf(condition: boolean; trueValue, falseValue: Color): Color;
begin
  if condition then Result := trueValue else Result := falseValue;
end;

function IntIf(condition: boolean; trueValue, falseValue: integer): integer;
begin
  if condition then Result := trueValue else Result := falseValue;
end;

function RealIf(condition: boolean; trueValue, falseValue: real): real;
begin
  if condition then Result := trueValue else Result := falseValue;
end;

function FormatMs(value: real): string;
begin
  Result := Format('{0:0.0} ms', value);
end;

function FormatPercent(value: real): string;
begin
  Result := Format('{0:0.0}%', value);
end;

procedure DrawTextWithBackground(bgColor: Color; x1, y1, x2, y2: integer; text: string; shadow: boolean);
begin
  Brush.Color := bgColor;
  Pen.Color := ARGB(80, 255, 255, 255);
  FillRoundRect(x1, y1, x2, y2, 14, 14);

  if shadow then
  begin
    Font.Color := ARGB(130, 0, 0, 0);
    DrawTextCentered(x1 + 2, y1 + 2, x2 + 2, y2 + 2, text);
  end;

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
