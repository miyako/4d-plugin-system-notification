//%attributes = {}
C_LONGINT:C283($1)

$path:=System folder:C487(Desktop:K41:16)+"test.txt"

If (Test path name:C476($path)=Is a document:K24:1)
	$doc:=Append document:C265($path)
Else 
	$doc:=Create document:C266($path)
End if 

SEND PACKET:C103($doc; String:C10($1)+"\r")

CLOSE DOCUMENT:C267($doc)

If ($1=SN On Before Machine Power Off)
	TRACE:C157
	QUIT 4D:C291
End if 