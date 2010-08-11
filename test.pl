#!/usr/bin/perl -w

foreach $sequitur ("./sequitur", "./sequitur_simple") {
    print "\nTesting $sequitur\n\n";

    test("overlapping digrams", 
	 "string",
	 "abbbabcbb", 
	 "0 -> 1 2 1 c 2 \n1 -> a b \n2 -> b b \n");
    
    test("repeating character",
	 "string",
	 "aaaaaa", 
	 "0 -> 1 1 1 \n1 -> a a \n");
    
    # from LI Xianfeng <lixianfe@comp.nus.edu.sg>
    test("long rules", 
	 "string",
	 "aacaacbcbdaacbcbdebcbd", 
	 "0 -> 1 2 2 e 3 \n1 -> a a c \n2 -> 1 3 \n3 -> b c b d \n");
    
    # head -c 10000 sequitur.tar.gz
    test("quasi-random",
	 "file",
	 "random.input",
	 "random.output");
    
    test("source code",
	 "file",
	 "code.input",
	 "code.output");
    
    # Windows EXE
    test("exe",
	 "file",
	 "exe.input",
	 "exe.output");
}

sub test {
    my($name, $type, $input, $desired_output) = @_;
    print $name, ' ' x (40 - length($name));
    
    $passed = 0;

    if ($type eq "string") {
	$output = `echo -n $input | $sequitur -pq`;
	$passed = $output eq $desired_output;
    } elsif ($type eq "file") {
	system("$sequitur -pq < testfiles/$input > /tmp/$$.test");
	$output = `cmp /tmp/$$.test testfiles/$desired_output`;
	$passed = $output eq "";
    } elsif ($type eq "compression") {
	system("$sequitur -cq < testfiles/$input > /tmp/$$.compressed");
	system("$sequitur -uq < /tmp/$$.compressed > /tmp/$$.test");
	$input_size = -s "testfiles/$input";
	$output_size = -s "/tmp/$$.compressed";
	$output = `cmp /tmp/$$.test testfiles/$input`;
	$passed = $output eq "";
    }

    if ($passed) {
	print "PASSED\n";
    } else {
	print "FAILED\n";
	$separator = '-' x 70;
	print "Expected $separator\n$desired_output$separator\n\nReceived $separator\n$output$separator\n";
    }

    if ($type eq "compression") {
	printf("$input_size in,\t$output_size out,\t%.2f bpc\n", $output_size / $input_size * 8);
    }

    if ($type eq "file" && $sequitur !~ /simple/) {
	test("$name (compression)", "compression", $input, "");
    }
}
