#!/bin/sh
echo "grab result"|nc localhost 8888|awk '
// {
  len[substr($1,3)]=length($3)/2;
}
END {
  print "# type (r[1-9];w;u),circuit,name,[comment],[QQ],ZZ,PBSB,[ID],field1,part (m/s),datatypes/templates"
  for (i in len) {
    zz=substr(i,1,2)
    pbsb=substr(i,3,4)
    id=substr(i,9)
    idlen=length(id)/2
    if (pbsb=="b509" && substr(id,1,2)=="0d") {
      //register read
      reg=strtonum("0x" substr(id,3,2))
      reg+=strtonum("0x" substr(id,5,2))*256
      print "r,unknown" zz ",reg" reg ",,," zz "," pbsb "," substr(id,3,4) ",data,,HEX:" (len[i]-1)
    } else if (pbsb=="b509" && substr(id,1,2)=="0e") {
      //register write
      reg=strtonum("0x" substr(id,3,2))
      reg+=strtonum("0x" substr(id,5,2))*256
      print "w,unknown" zz ",reg" reg ",,," zz "," pbsb "," substr(id,3,4) ",data,,HEX:" (idlen-3)
    } else if (pbsb=="b509" && substr(id,1,2)=="29") {
      //register update
      reg=strtonum("0x" substr(id,3,2))
      reg+=strtonum("0x" substr(id,5,2))*256
      print "u,unknown" zz ",reg" reg ",,," zz "," pbsb "," substr(id,3,4) ",,,IGN:2,,,,data,,HEX:" (len[i]-3)
    } else if (len[i]<=1 || idlen>3) {
      //seems to be a write
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
'
