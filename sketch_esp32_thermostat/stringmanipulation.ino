

void time_and_date_string(String & s, time_t t)
{
      s +=  String((hour(t))) + ":";
      s +=  String((minute(t))) + ":";
      s +=  String((second(t)))  + "  ";

      s +=  String((month(t))) + "/";
      s +=  String((day(t))) + "/";
      s +=  String((year(t))) ;
}

void unix_time_string (String & s, time_t t)
{
   s+= String(t);
}

void json_HTTP_header(String & s)
  {
      s += "HTTP/1.1 200 OK\n";
      s += "Access-Control-Allow-Origin: *\n";
      s += "Content-Type: application/json\n";
      s += "\n"; //This carry return is very important. Without it the response will not be sent.
  }


