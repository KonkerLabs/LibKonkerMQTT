void parseURLtoURIePath(String URL, String &URI, String &Path, String &Port){
  int doubleSlash=URL.indexOf("//",0);
  int doublePoint=-1;
  int firstPathSlash=-1;
  String strPort="";
  if (doubleSlash>=0){
    firstPathSlash=URL.indexOf("/",doubleSlash+2);
    doublePoint=URL.indexOf(":",doubleSlash+2);
  }else{
    firstPathSlash=URL.indexOf("/",0);
    doublePoint=URL.indexOf(":",0);
  }

  if(doublePoint>=0){
    if(firstPathSlash>=0){
      strPort=URL.substring(doublePoint+1,firstPathSlash);
    }else{
      strPort=URL.substring(doublePoint+1,URL.length());
    }
  }


  Port=strPort;
  if(firstPathSlash>=0){
    if(doublePoint>0){
      Path=URL.substring(firstPathSlash,URL.length());
      URI=URL.substring(0,firstPathSlash-strPort.length()-1);
    }else{
      Path=URL.substring(firstPathSlash,URL.length());
      URI=URL.substring(0,firstPathSlash);
    }

  }else{
    Path="";
    if(doublePoint>0){
      URI=URL.substring(0,doublePoint);
    }else{
      URI=URL;
    }
  }

}




void parseURLtoURIePath(String URL, String &URI, String &Path){
  String p;
  parseURLtoURIePath(URL, URI, Path, p);
}
