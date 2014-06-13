bool _ABVAR_1_value;

void setup()
{
Serial.begin(9600);
pinMode( 2 , OUTPUT);
_ABVAR_1_value = false;
}

void loop()
{
digitalWrite( 2 , _ABVAR_1_value );
Serial.print( analogRead(A1) );
Serial.println("");
_ABVAR_1_value = !( _ABVAR_1_value ) ;
}


