PROGRAM = sequitur

JAVA = javac -O

SOURCES = symbol.java guard.java terminal.java nonTerminal.java \
	  rule.java sequitur.java

CLASSFILES = $(SOURCES:.java=.class)

%.class : %.java
	$(JAVA) $< 

#-------------------------------------------------------------------------------#

clean : 
	$(RM) $(CLASSFILES) $(PROGRAM).class 

$(PROGRAM) : $(CLASSFILES) $(PROGRAM).class

