#!/usr/bin/perl -w

$additional_args = "r"; # print full expansion after each rule
#$additional_args = "rt"; # print full expansion and number of references after each rule

use Getopt::Std;

my($opt_h);
getopts("f:e:1h");

if (!$opt_f || $opt_h) {
    print <<END;

line_sequitur.pl
-----------

-f <file>    input file
-d <word>    delimiter line (don\'t form rules across line)
-1           remove hapax legomenae (default is to include them)
-h           this help

Runs sequitur on a line by line basis by transforming lines into numbers
and using sequitur -d to treat numbers as symbols.

END
exit(0);
}


$line_number = 1;
if ($opt_e) {
  $delimiter = $opt_e;
  $additional_args .= " -e 1";
  $number{$delimiter} = 1;
  $line[1] = $delimiter;
  $line_number = 2;
}

open(IN, "<$opt_f");
open(SEQUITUR, "| ./sequitur -dp$additional_args > $opt_f.tmp");

while (<IN>) {
    chop($line = $_);
    
    if (defined($number{$line})) { 
      print SEQUITUR $number{$line}, "\n"; 
    }	
    else {
      $number{$line} = $line_number;
      $line[$line_number] = $line;
      print SEQUITUR $line_number, "\n";
      $line_number ++;
    }
}

close(IN);
close(SEQUITUR);

open(TMP, "$opt_f.tmp");

while (<TMP>) {
  s/\[(\d+)\]/$line[$1]\\n/g;
  print;
}
