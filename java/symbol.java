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

import java.util.Hashtable;

public abstract class symbol{

  static final int numTerminals = 100000;

  static final int prime = 2265539;
  static Hashtable theDigrams = new Hashtable(symbol.prime);
  
  public int value;
  symbol p,n;
  
  /**
   * Links two symbols together, removing any old
   * digram from the hash table.
   */
  
  public static void join(symbol left, symbol right){
    if (left.n != null) 
      left.deleteDigram();
    left.n = right;
    right.p = left;
  }

  /**
   * Abstract method: cleans up for symbol deletion.
   */

  public abstract void cleanUp();

  /**
   * Inserts a symbol after this one.
   */

  public void insertAfter(symbol toInsert){
    join(toInsert,n);
    join(this,toInsert);
  }

  /**
   * Removes the digram from the hash table.
   * Overwritten in sub class guard.
   */

  public void deleteDigram(){
    
    symbol dummy;

    if (n.isGuard())
      return;
    dummy = (symbol)theDigrams.get(this);

    // Only delete digram if its exactly
    // the stored one.

    if (dummy == this)
      theDigrams.remove(this);
  }

  /**
   * Returns true if this is the guard symbol.
   * Overwritten in subclass guard.
   */

  public boolean isGuard(){
    return false;
  }

  /**
   * Returns true if this is a non-terminal.
   * Overwritten in subclass nonTerminal.
   */

  public boolean isNonTerminal(){
    return false;
  }

  /**
   * Checks a new digram. If it appears
   * elsewhere, deals with it by calling
   * match(), otherwise inserts it into the
   * hash table.
   * Overwritten in subclass guard.
   */

  public boolean check(){
    
    symbol found;

    if (n.isGuard())
      return false;
    if (!theDigrams.containsKey(this)){
      found = (symbol)theDigrams.put(this,this);
      return false;
    }
    found = (symbol)theDigrams.get(this);
    if (found.n != this)
      match(this,found);
    return true;
 }

  /**
   * Replace a digram with a non-terminal.
   */

  public void substitute(rule r){
    cleanUp();
    n.cleanUp();
    p.insertAfter(new nonTerminal(r));
    if (!p.check())
      p.n.check();
  }

  /**
   * Deal with a matching digram.
   */
  
  public void match(symbol newD,symbol matching){
    
    rule r;
    symbol first,second,dummy;
    
    if (matching.p.isGuard() && 
	matching.n.n.isGuard()){
      
      // reuse an existing rule
      
      r = ((guard)matching.p).r;
      newD.substitute(r);
    }else{
      
      // create a new rule
      
      r = new rule();
      try{
	first = (symbol)newD.clone();
	second = (symbol)newD.n.clone();
	r.theGuard.n = first;
	first.p = r.theGuard;
	first.n = second;
	second.p = first;
	second.n = r.theGuard;
	r.theGuard.p = second;
	
	dummy = (symbol)theDigrams.put(first,first);
	matching.substitute(r);
	newD.substitute(r);
      }catch (CloneNotSupportedException c){
	c.printStackTrace();
      }
    }
    
    // Check for an underused rule.
    
    if (r.first().isNonTerminal() && 
	(((nonTerminal)r.first()).r.count == 1))
      ((nonTerminal)r.first()).expand();
  }
  
  /**
   * Produce the hashcode for a digram.
   */

  public int hashCode(){
    
    long code;

    // Values in linear combination with two
    // prime numbers.

    code = ((21599*(long)value)+(20507*(long)n.value));
    code = code%(long)prime;
    return (int)code;
  }

  /**
   * Test if two digrams are equal.
   * WARNING: don't use to compare two symbols.
   */

  public boolean equals(Object obj){
    return ((value == ((symbol)obj).value) &&
	    (n.value == ((symbol)obj).n.value));
  }
}









