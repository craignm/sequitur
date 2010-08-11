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

import java.awt.*;

public class sequitur extends java.applet.Applet {
  
  TextArea text;
  TextArea rules;
  Button submit;
  Panel dataPanel;
  Panel rulesPanel;
  Panel buttonPanel;
  Panel label1Panel;
  Panel label2Panel;
  Label dataLabel;
  Label rulesLabel;
  Font f1 = new Font("TimesRoman", Font.BOLD, 18);
  Font f2 = new Font("TimesRoman", Font.PLAIN,12);
  
  public void runSequitur(){
    
    rule firstRule = new rule();
    int i;

    // Reset number of rules and Hashtable.

    rule.numRules = 0;
    symbol.theDigrams.clear();
    for (i=0;i<text.getText().length();i++){
      firstRule.last().
	insertAfter(new terminal(text.getText().
				 charAt(i)));
      firstRule.last().p.check();
    }
    rules.setText(firstRule.getRules());
  }

  public boolean action(Event evt,Object arg){
    if (evt.target == submit){
      submit.disable();
      text.setEditable(false);
      runSequitur();
      submit.enable();
      text.setEditable(true);
    }
    return true;
  }

  public void init(){

    String defaultText = new String("pease porridge hot,\npease porridge cold,\npease porridge in the pot,\nnine days old.\n\nsome like it hot,\nsome like it cold,\nsome like it in the pot,\nnine days old.\n");

    setLayout(new FlowLayout());
    dataPanel = new Panel();
    dataPanel.setLayout(new BorderLayout());
    add(dataPanel);
    label1Panel = new Panel();
    label1Panel.setLayout(new FlowLayout());
    dataPanel.add("North",label1Panel);
    dataLabel = new Label("Data");
    dataLabel.setFont(f1);
    label1Panel.add(dataLabel);
    text = new TextArea(9,70);
    text.setFont(f2);
    text.setText(defaultText);
    dataPanel.add("South",text);
    rulesPanel = new Panel();
    rulesPanel.setLayout(new BorderLayout());
    add(rulesPanel);
    label2Panel = new Panel();
    label2Panel.setLayout(new FlowLayout());
    rulesPanel.add("North",label2Panel);
    rulesLabel = new Label("Grammar");
    rulesLabel.setFont(f1);
    label2Panel.add(rulesLabel);
    rules = new TextArea(9,70);
    rules.setEditable(false);
    rules.setFont(f2);
    rulesPanel.add("South",rules);
    buttonPanel = new Panel();
    buttonPanel.setLayout(new FlowLayout());
    add(buttonPanel);
    submit = new Button("Run sequitur");
    submit.setFont(f1);
    buttonPanel.add(submit);
   }
}
