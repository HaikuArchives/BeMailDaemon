#!/bin/sh
# ValidatePeopleEmails
# Goes through all your People files and checks that the e-mail addresses
# are valid.  Useful for bulk e-mailing where one bad address will make
# the SMTP mail server reject the whole lot.
# By mmu_man, September 18 2003, inspired by Dane's request, used with
# permission in MDR.

query -a 'META:email==*' | xargs catattr META:email | while read LINE; do
	NULLATTR="${LINE#* : string :}"
	if [ "x$NULLATTR" = "x" ]; then
		continue
	fi
	PPLFILE="${LINE% : string : *}"
	PPL="${PPLFILE##*/}"
	THEMAIL="${LINE#* : string : }"
	FILTERED="$(echo "$THEMAIL" | sed 's/[A-Za-z0-9.+_-]*\@[A-Za-z0-9._-]*/GOOD/' )"
	echo "[$PPL] [$THEMAIL] $FILTERED"
	if [ "x$FILTERED" != "xGOOD" ]; then
		echo "WARNING: strange email for '$PPL': <$THEMAIL>"
	fi
done
