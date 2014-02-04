#!/usr/bin/perl -w 

use strict; 
use IO::Socket; 
use Getopt::Long;
use HTML::Entities;

use POSIX qw( strftime );

my $config = {};

sub debug
{
	my $title = shift || return;
	my $additional = shift;

	if ($config->{debug}) 
	{
		print "[ $title ]\n";
		if (defined $additional) 
		{
			my @ar = split("\n", $additional);
			foreach my $s (@ar) 
			{
				print "\t$s\n";
			}
		}
	}
}

sub open_output
{
	my ($context) = shift || return 0;
	my ($fn) = shift || return 0;

#	$fn = fix_filename($fn);
	open(OUTPUT, ">$fn") || die "FIXME: $fn";
	OUTPUT->autoflush(1);
	binmode OUTPUT;
	$context->{output_open} = 1;
	return 1;
}

sub close_output_stream
{
	my $context = shift || return;

	# close old output stream
	$context->{output_open} = 0;
	close OUTPUT;
}

# write block to disk
sub write_block
{
	my ($chunk) = shift || return;
	my ($context) = shift || return;
	
	# open output if there is none
	if ($context->{output_open} == 0) 
	{
		#timestamp the file
		#my $fn = strftime("%Y-%d-%m-%H-%M-%S", localtime);
		#my $fn = strftime("%Y%m%d%H%M%S", localtime);

		my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst)=localtime(time());
		my $fn = sprintf "%04d-%02d-%02d-%02d-%02d-%02d", $year+1900,$mon+1,$mday,$hour,$min,$sec;

		$fn .= ".mp3";
		$fn = $config->{'path'}.$fn;

		print STDERR "filename ".$fn;

		return unless open_output($context, $fn);
	}

	# write chunk to output
	if ($context->{output_open} == 1) 
	{
		print OUTPUT $chunk;
	}
}

sub loop_pipe
{
	my ($context) = {};
	my $block_count=0;

	$context->{output_open}=0;

	while(1) 
	{
		read(STDIN, my $chunk, 6000);

		print STDERR "Read ".length($chunk)." bytes\n";
		if(length($chunk)>0)
		{
			$context->{length} += length($chunk);
			write_block($chunk, $context);

			$block_count+=length($chunk);
			
			# ~1Mb files
			if($block_count>6000*166)
			{
				print STDERR "Writing $block_count bytes\n";
				close_output_stream($context);
				$block_count=0;
			}
		}
		else
		{
			print STDERR "Disconnected\n";
			return 0;
		}
	}
	
	return 1;
}

sub trim
{
	my ($str) = shift || return undef;
	
	$str =~ s/^[\s\t]//g;
	$str =~ s/[\s\t]$//g;
	return $str;
}

my (%options) = ();
GetOptions(\%options, "--path=s");
$config->{'path'} = (defined $options{'path'}) ? $options{'path'} : die "Define output path using --path"; 

print STDERR "Starting...\n";
loop_pipe();
print STDERR "Finished.\n";
exit;
