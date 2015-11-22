#!/bin/sh
ebusctl grab result|awk '
BEGIN{
  len[""]=0
  delete len[""]
  found=0
}
/grab disabled/ {
  print "enable grab first!"
  exit
}
/.+/ {
  len[substr($1,3)]=length($3)/2;
  found=1
}
END {
  if (found>0)
    print "# type (r[1-9];w;u),circuit,name,[comment],[QQ],ZZ,PBSB,[ID],field1,part (m/s),datatypes/templates,divider/values,unit,comment,field2,part (m/s),datatypes/templates"
  for (i in len) {
    zz=substr(i,1,2)
    pbsb=substr(i,3,4)
    id=substr(i,9)
    idlen=length(id)/2
    if (pbsb=="b509" && substr(id,1,2)=="0d") {
      # register read
      reg=index("123456789abcdef",tolower(substr(id,4,1)))
      reg+=index("123456789abcdef",tolower(substr(id,3,1)))*16
      reg+=index("123456789abcdef",tolower(substr(id,6,1)))*256
      reg+=index("123456789abcdef",tolower(substr(id,5,1)))*4096
      print "r,unknown" zz ",reg" reg ",,," zz "," pbsb "," substr(id,1,6) ",data,,HEX:" (len[i]-1)
    } else if (pbsb=="b509" && substr(id,1,2)=="0e") {
      # register write
      reg=index("123456789abcdef",tolower(substr(id,4,1)))
      reg+=index("123456789abcdef",tolower(substr(id,3,1)))*16
      reg+=index("123456789abcdef",tolower(substr(id,6,1)))*256
      reg+=index("123456789abcdef",tolower(substr(id,5,1)))*4096
      print "w,unknown" zz ",reg" reg ",,," zz "," pbsb "," substr(id,1,6) ",data,,HEX:" (idlen-3)
    } else if (pbsb=="b509" && substr(id,1,2)=="29") {
      # register update
      reg=index("123456789abcdef",tolower(substr(id,4,1)))
      reg+=index("123456789abcdef",tolower(substr(id,3,1)))*16
      reg+=index("123456789abcdef",tolower(substr(id,6,1)))*256
      reg+=index("123456789abcdef",tolower(substr(id,5,1)))*4096
      print "u,unknown" zz ",reg" reg ",,," zz "," pbsb "," substr(id,1,6) ",,,IGN:2,,,,data,,HEX:" (len[i]-3)
    } else if (len[i]<=1 || idlen>3) {
      # seems to be a write
      if (idlen>3) idlen=3
      else if (len[i]<=1) idlen--;
      printf "w,unknown" zz ","
      if (substr(pbsb,1,2)=="b5") printf "block" substr(pbsb,3,2)
      else printf pbsb
      printf substr(id,1,idlen*2) ",,," zz "," pbsb "," substr(id,1,idlen*2)
      if (length(id)/2>idlen)
        printf ",data,,HEX:" (length(id)/2-idlen)
      if (len[i]>1)
        printf ",result,s,HEX:" (len[i]-1)
      print ""
    } else {
      printf "r,unknown" zz ","
      if (substr(pbsb,1,2)=="b5") printf "block" substr(pbsb,3,2)
      else printf pbsb
      print id ",,," zz "," pbsb "," id ",data,,HEX:" (len[i]-1)
    }
  }
}
'|uniq
