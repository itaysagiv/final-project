def genFasta():
	fasta = open("Keys.fa","w")
	dumpFile = open("Keys.txt","r")
	seqIdx = -1
	for line in dumpFile.readlines():
		if "===" in line:
			seqIdx+=1
			fasta.write(">SEQUENCE_{}\n".format(seqIdx))
			continue
		else:
			line = line.replace("1","A")
			line = line.replace("0","C")
			line = line.replace("?","-")
			fasta.write(line)
	fasta.close()
	dumpFile.close()

genFasta()
