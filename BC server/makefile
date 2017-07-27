JFLAGS = -g
JC = javac
.SUFFIXES: .java .class
.java.class:
	$(JC) $(JFLAGS) $*.java

CLASSES = \
	RunExponents.java \
	RSACoreEngine.java \
	BigInteger.java \
	MutableBigInteger.java \
	SignedMutableBigInteger.java

default: classes

classes: $(CLASSES:.java=.class)

clean:
	$(RM) *.class
