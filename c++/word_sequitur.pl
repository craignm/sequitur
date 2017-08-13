#!/usr/bin/perl -w

$additional_args = "r"; # print full expansion after each rule
#$additional_args = "rt"; # print full expansion and number of references after each rule

use Getopt::Std;

my($opt_h);
getopts("f:e:1h");

if (!$opt_f || $opt_h) {
    print <<END;

word_sequitur.pl
-----------

-f <file>    input file
-d <word>    delimiter word (don\'t form rules across word)
-1           remove hapax legomenae (default is to include them)
-h           this help

Runs sequitur on a word by word basis by transforming words into numbers
and using sequitur -d to treat numbers as symbols.

END
exit(0);
}


# don't  include hapax legomenae

if ($opt_1) {
  open(IN, "<$opt_f");
  while (<IN>) {
    chop($line = lc($_));
    
    foreach $word (split(/[^a-z0-9]+/, $line)) {
      $count{$word} ++;
    }
  }
  $count{'br'} = 0;
  $count{'p'} = 0;
  
  close(IN);
}

$word_number = 1;
if ($opt_e) {
  $delimiter = lc($opt_e);
  if ($delimiter =~ /[^a-z0-9]/) {
    print STDERR "Delimiter word can only contain a-z and 0-9\n";
    exit(1);
  }
  $additional_args .= " -e 1";
  $number{$delimiter} = 1;
  $word[1] = $delimiter;
  $word_number = 2;
}

open(IN, "<$opt_f");
open(SEQUITUR, "| ./sequitur -dp$additional_args > $opt_f.tmp");

while (<IN>) {
    chop($line = lc($_));
    
    foreach $word (split(/[^a-z0-9]+/, $line)) {
	if ((!$opt_1 || $count{$word} > 1) && $word ne "") {
          if (defined($number{$word})) { 
            print SEQUITUR $number{$word}, "\n"; 
          }	
	    else {
		$number{$word} = $word_number;
		$word[$word_number] = $word;
		print SEQUITUR $word_number, "\n";
                $word_number ++;
	    }
	}
    }
}
close(IN);
close(SEQUITUR);

open(TMP, "$opt_f.tmp");

while (<TMP>) {
  s/\[(\d+)\]/$word[$1]/g;
  print;
}
