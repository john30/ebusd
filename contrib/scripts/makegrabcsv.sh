#!/bin/sh
port=8888
if [ "x$1" = "x-p" ]; then
  shift
  port=$1
  shift
fi
maxidlen=3
if [ "x$1" = "x-m" ]; then
  shift
  maxidlen=$1
fi
echo "grab result"|nc localhost $port|awk -vmaxidlen=$maxidlen '
BEGIN{
  sdatas[""]=0
  delete sdatas[""]
  sdatasorder[""]=0
  delete sdatasorder[""]
  FS="/"
}
/grab disabled/ {
  print "enable grab first!"
  exit
}
/empty/ {
  print "empty"
  exit
}
/.+/ {
  m=gensub(" ", "", "g", gensub("=.*$", "", "", $1))
  s=gensub(" ", "", "g", gensub("=.*$", "", "", $2))
  pbsbnnid=substr(m,5,(3+maxidlen)*2)
  if (!shorted[pbsbnnid]) {
    shorted[pbsbnnid]=1
    sdatas[substr(m,3)]=s
    sdatasorder[length(sdatasorder)]=substr(m,3)
  }
}
function hextodec(str,pos) {
  if (length(str)<pos+1) return -1
  return index("123456789abcdef",substr(str,pos,1))*16+index("123456789abcdef",substr(str,pos+1,1))
}
function ismaster(zz) {
  z=substr(zz,1,1)
  if (z!="0" && z!="1" && z!="3" && z!="7" && z!="f") return false
  z=substr(zz,2,1)
  if (z!="0" && z!="1" && z!="3" && z!="7" && z!="f") return false
  return true
}
END {
  if (length(sdatas)>0)
    print "# type (r[1-9];w;u),circuit,name,[comment],[QQ],ZZ,PBSB,[ID],field1,part (m/s),datatypes/templates,divider/values,unit,comment,field2,part (m/s),datatypes/templates"
  for (i=0; i<length(sdatasorder); i++) {
    mdata=sdatasorder[i]
    sdata=sdatas[mdata]
    zz=substr(mdata,1,2)
    pbsbdd=substr(mdata,3,4) substr(mdata,9,2)
    pbsb=substr(mdata,3,4)
    id=substr(mdata,9)
    idlen=length(id)/2
    if (idlen>maxidlen) {
      idlen=maxidlen
      master=substr(id,1+idlen*2)
      id=substr(id,1,idlen*2)
    } else {
      master=""
    }
    masterlen=length(master)/2
    sdata=sdatas[mdata]
    if (sdata=="") {
      slave=""
      slavelen=0
    } else {
      slave=substr(sdata,3)
      slavelen=length(slave)/2
    }
    if (idlen>=3 && pbsbdd=="b5090d") {
      # vaillant register read
      reg=hextodec(id,5)*256+hextodec(id,3)
      print "# " mdata " / " sdata ": reg=" reg ", data=" slave
      printf "r,unknown" zz ",reg" reg ",,," zz "," pbsb "," id
      print ",data,s,HEX:" slavelen ",,,"
    } else if (idlen>=3 && pbsbdd=="b5090e") {
      # vaillant register write
      reg=hextodec(id,5)*256+hextodec(id,3)
      print "# " mdata " / " sdata ": reg=" reg ", data=" master (slavelen>0 ? ", result=" slave : "")
      printf "w,unknown" zz ",reg" reg ",,," zz "," pbsb "," id
      printf ",data,m,HEX:" masterlen ",,,"
      if (slavelen>0)
        printf ",result,s,HEX:" slavelen
      print ""
    } else if (idlen>=3 && pbsbdd=="b50929") {
      # vaillant register update
      reg=hextodec(id,5)*256+hextodec(id,3)
      print "# " mdata " / " sdata ": reg=" reg ", data=" substr(slave,5)
      printf "u,unknown" zz ",reg" reg ",,," zz "," pbsb "," id
      print ",,,IGN:2,,,,data,,HEX:" (slavelen-2)
    } else if (slavelen<=0 || masterlen>0) {
      # generic write or write+read
      if (masterlen<=0 && slavelen<=0) { # no slave data and no master data: move one byte from id to master data
        idlen--
        masterlen++
        master=substr(id,1+idlen*2) master
        id=substr(id,1,idlen*2)
      }
      print "# " mdata " / " sdata ": pbsb=" pbsb ", id=" id (masterlen>0 ? ", data=" master : "") (slavelen>0 ? ", result=" slave : "")
      if (zz=="fe") printf "w,broadcast,"
      else printf "w,unknown" zz ","
      if (substr(pbsb,1,2)=="b5") printf "block" substr(pbsb,3,2)
      else printf pbsb
      printf id ",,," zz "," pbsb "," id
      if (masterlen>0)
        printf ",data,m,HEX:" masterlen ",,,"
      if (slavelen>0)
        printf ",result,s,HEX:" slavelen
      print ""
    } else {
      # generic read
      print "# " mdata " / " sdata ": pbsb=" pbsb ", id=" id ", data=" slave
      printf "r,unknown" zz ","
      if (substr(pbsb,1,2)=="b5") printf "block" substr(pbsb,3,2)
      else printf pbsb
      print id ",,," zz "," pbsb "," id ",data,s,HEX:" slavelen
    }
  }
}
'
