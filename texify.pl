#! /usr/bin/perl

while ($_ = shift) {
    scan_file($_, 0);
}
#for ($i=1; $ARGV[$i]; $i++) {
#    scan_file($ARGV[$i], 1);
#}
#scan_file($ARGV[0], 0);

sub scan_file {

    local $filename = shift; 
    local $definitions_only = shift;

    open(FILE, $filename);

    local $line = 0;
    local $header_symbol = 'C';
    local $first_code_line = 0;

    print_header($filename);
    
    while ( <FILE> ) {

	$line++;

	if ( /^\s*\/\*([DMISCH])/ ) {

	    if ( $header_symbol eq 'H' ) {
		print $header_text;
	    } elsif ( $header_symbol eq 'D' ) {

		$text =~ s/^[\s\n]*\n//;
		$text =~ s/[\s\n]*$/\n/;

		$header_text =~ s/\#([\w\_\-\.\>]+)/\\verb!$1!/go;
		$header_text =~ s/\"([\w\_\-\.\>]+)\"/{\\em $1}/go;

		$tag_header{$tag} = $header_text;

		#print "$tag -> $tag_header{$tag}";

		$tag_body{$tag} = $text;
		$tag_filename{$tag} = $filename;
		$tag_first_code_line{$tag} = $first_code_line;
		$tag_code_lines{$tag} = $code_lines;
		$tag = "";
	    } elsif ( $definitions_only ) {

	    } elsif ( $header_symbol eq 'I' ) {

		$text =~ s/^[\s\n]*\n//;
		$text =~ s/[\s\n]*$/\n/;

		$header_text =~ s/\#([\w\_\-\.\>]+)/\\verb!$1!/go;
		$header_text =~ s/\"([\w\_\-\.\>]+)\"/{\\em $1}/go;

		print_info($tag_filename{$tag},
			   $tag_first_code_line{$tag},
			   $tag_first_code_line{$tag} + $tag_code_lines{$tag},
			   $header_text ."\n". $tag_header{$tag},
			   $tag_body{$tag});

		if ( $text =~ /\S/ ) {

		    print_info($filename,
			       $first_code_line,
			       $first_code_line + $code_lines,
			       "",
			       $text);
		}

	    } elsif ( $header_symbol eq 'M' ) {

		$text =~ s/^[\s\n]*\n//;
		$text =~ s/[\s\n]*$/\n/;

		$header_text =~ s/\#([\w\_\-\.\>]+)/{\\small\\em\\verb!$1!}/go;
		$header_text =~ s/\"([^\"]+)\"/{\\em $1}/go;

		print_info($filename,
			   $first_code_line,
			   $first_code_line + $code_lines,
			   $header_text, 
			   $text);
	    
	    } elsif ( $header_symbol eq 'S' ) {

		$text =~ s/^[\s\n]*\n//;
		$text =~ s/[\s\n]*$/\n/;

		$header_text =~ s/^[\s\n]*//;
		$header_text =~ s/^(\S.*)//;
		print "\\subsection{$1}\n";
		$header_text =~ s/^[\s\n]*//;

		$header_text =~ s/\#([\w\_\-\.\>]+)/\\verb!$1!/go;
		$header_text =~ s/\"([\w\_\-\.\>]+)\"/{\\em $1}/go;

		print_info($filename,
			   $first_code_line,
			   $first_code_line + $code_lines,
			   $header_text, 
			   $text);
	    }

	    if ( $1 eq 'D' ) {
		$in_insert_definition = 1;
		$tag = "";
	    } else {
		$in_insert_definition = 0;
	    }
	    $header_symbol = $1;
	    $in_header = 1;
	    $text = "";
	} elsif ( /^\s*\/\*([^\s@])/ ) {
	    print "Unknown literal $1 in $ARGV[$i]. on $line\n";
	    exit(0);
	} elsif ( /^\s*\@(\S+)\s/ ) {
	    $tag = $1;
	} elsif ( m/\*\*\/\s*$/ ) {
	    $header_text = $text;
	    $first_code_line = $line + 1;
	    $in_header = 0;
	    $text = "";
	    $code_lines = -1;
	    $code_lines_pre = 0;
	} else {
	    $text .= $_;
	    if ( $text =~ /\S/ ) { 
		$code_lines_pre++;
		if ( $_ =~ /\S/ ) {
		    $code_lines += $code_lines_pre; 
		    $code_lines_pre = 0;
		} 
	    } else {
		$first_code_line++; 
	    }
	}
    }

    if ( $tag =~ /\S/ ) {
	$tag_header{$tag} = $header_text;
	$tag_body{$tag} = $text;
	$tag_filename{$tag} = $filename;
	$tag_first_code_line{$tag} = $first_code_line;
    }

    close(FILE);
}

sub print_info {

    local $filename = shift;
    local $first_code_line = shift;
    local $last_code_line = shift;
    local $header_text = shift;
    local $text = shift;

    $filename =~ s/\_/\\\_/go;

    print "$header_text";
    if ( $text =~ m/\S/ ) {
	print "\\vspace{5mm}\n\\begin{lstlisting}".
	    "[title={\\raisebox{2mm}[0pt][0pt]{".
	    "\\hspace{9cm}\\footnotesize $filename, ".
	    "{\\em lines $first_code_line - $last_code_line}}},".
	    "firstnumber=$first_code_line]{}\n".
	    "$text\\end{lstlisting}\n";
    }
}

sub print_header {
    local $filename = shift;

    use File::Basename;
    my $file = basename($filename);
    
    print "\\section{$file}\n\\label{code:$file}\n\n";
}
