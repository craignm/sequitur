/*
This class is part of a Java port of Craig Nevill-Manning's Sequitur algorithm.
Copyright (C) 1997 Eibe Frank

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

import java.util.Vector;

public class rule{

  // Guard symbol to mark beginning
  // and end of rule.

  public guard theGuard;

  // Counter keeps track of how many
  // times the rule is used in the
  // grammar.

  public int count;

  // The total number of rules.

  public static int numRules = 0;

  // The rule's number.
  // Used for identification of
  // non-terminals.

  public int number;

  // Index used for printing.

  public int index;

  rule(){
    number = numRules;
    numRules++;
    theGuard = new guard(this);
    count = 0;
    index = 0;
  }

  public symbol first(){
    return theGuard.n;
  }
  
  public symbol last(){
    return theGuard.p;
  }

  public String getRules(){
    
    Vector rules = new Vector(numRules);
    rule currentRule;
    rule referedTo;
    symbol sym;
    int index;
    int processedRules = 0;
    StringBuffer text = new StringBuffer();
    int charCounter = 0;

    text.append("Usage\tRule\n");
    rules.addElement(this);
    while (processedRules < rules.size()){
      currentRule = (rule)rules.elementAt(processedRules);
      text.append(" ");
      text.append(currentRule.count);
      text.append("\tR");
      text.append(processedRules);
      text.append(" -> ");
      for (sym=currentRule.first();(!sym.isGuard());sym=sym.n){
	if (sym.isNonTerminal()){
	  referedTo = ((nonTerminal)sym).r;
	  if ((rules.size() > referedTo.index) &&
	      ((rule)rules.elementAt(referedTo.index) ==
	       referedTo)){
	    index = referedTo.index;
	  }else{
	    index = rules.size();
	    referedTo.index = index;
	    rules.addElement(referedTo);
	  }
	  text.append('R');
	  text.append(index);
	}else{
	  if (sym.value == ' '){
	    text.append('_');
	  }else{
	    if (sym.value == '\n'){
	      text.append("\\n");
	    }else
	      text.append((char)sym.value);
	  }
	}
	text.append(' ');
      }
      text.append('\n');
      processedRules++;
    }
    return new String(text);
  }
}
