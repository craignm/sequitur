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

public class nonTerminal extends symbol implements Cloneable{

  rule r;

  nonTerminal(rule theRule){
    r = theRule;
    r.count++;
    value = numTerminals+r.number;
    p = null;
    n = null;
  }

  /**
   * Extra cloning method necessary so that
   * count in the corresponding rule is
   * increased.
   */

  protected Object clone(){

    nonTerminal sym = new nonTerminal(r);

    sym.p = p;
    sym.n = n;
    return sym;
  }

  public void cleanUp(){
    join(p,n);
    deleteDigram();
    r.count--;
  }

  public boolean isNonTerminal(){
    return true;
  }

  /**
   * This symbol is the last reference to
   * its rule. The contents of the rule
   * are substituted in its place.
   */

  public void expand(){
    join(p,r.first());
    join(r.last(),n);

    // Necessary so that garbage collector
    // can delete rule and guard.

    r.theGuard.r = null;
    r.theGuard = null;
  }
}
