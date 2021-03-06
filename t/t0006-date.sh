#!/bin/sh

test_description='test date parsing and printing'
. ./test-lib.sh

# arbitrary reference time: 2009-08-30 19:20:00
TEST_DATE_NOW=1251660000; export TEST_DATE_NOW

check_show() {
	t=$(($TEST_DATE_NOW - $1))
	echo "$t -> $2" >expect
	test_expect_${3:-success} "relative date ($2)" "
	test-date show $t >actual &&
	test_i18ncmp expect actual
	"
}

check_show 5 '5 seconds ago'
check_show 300 '5 minutes ago'
check_show 18000 '5 hours ago'
check_show 432000 '5 days ago'
check_show 1728000 '3 weeks ago'
check_show 13000000 '5 months ago'
check_show 37500000 '1 year, 2 months ago'
check_show 55188000 '1 year, 9 months ago'
check_show 630000000 '20 years ago'
check_show 31449600 '12 months ago'
check_show 62985600 '2 years ago'

check_parse() {
	echo "$1 -> $2" >expect
	test_expect_${4:-success} "parse date ($1${3:+ TZ=$3})" "
	TZ=${3:-$TZ} test-date parse '$1' >actual &&
	test_cmp expect actual
	"
}

check_parse 2008 bad
check_parse 2008-02 bad
check_parse 2008-02-14 bad
check_parse '1582-02-14 20:30:45' bad
check_parse '2008-02-14 20:30:45' '2008-02-14 20:30:45 +0000'
check_parse '2008-02-14 20:30:45 -0500' '2008-02-14 20:30:45 -0500'
check_parse '2008-02-14 20:30:45 -0015' '2008-02-14 20:30:45 -0015'
check_parse '2008-02-14 20:30:45 -5' '2008-02-14 20:30:45 +0000'
check_parse '2008-02-14 20:30:45 -5:' '2008-02-14 20:30:45 +0000'
check_parse '2008-02-14 20:30:45 -05' '2008-02-14 20:30:45 -0500'
check_parse '2008-02-14 20:30:45 -:30' '2008-02-14 20:30:45 +0000'
check_parse '2008-02-14 20:30:45 -05:00' '2008-02-14 20:30:45 -0500'
check_parse '2008-02-14 20:30:45' '2008-02-14 20:30:45 -0500' EST5
check_parse '1960-02-14 20:30:45 -0500' '1960-02-14 20:30:45 -0500'
check_parse '1112911993 -0700' '2005-04-07 15:13:13 -0700'
check_parse '@-1112911993 -0700' '1934-09-25 18:46:47 -0700'

check_approxidate_relative() {
	echo "$1 -> $2 +0000" >expect
	test_expect_${3:-success} "parse approxidate_relative ($1)" "
	test-date approxidate_relative '$1' >actual &&
	test_cmp expect actual
	"
}

check_approxidate_relative now '2009-08-30 19:20:00'
check_approxidate_relative '5 seconds ago' '2009-08-30 19:19:55'
check_approxidate_relative 5.seconds.ago '2009-08-30 19:19:55'
check_approxidate_relative 10.minutes.ago '2009-08-30 19:10:00'
check_approxidate_relative yesterday '2009-08-29 19:20:00'
check_approxidate_relative 3.days.ago '2009-08-27 19:20:00'
check_approxidate_relative 3.weeks.ago '2009-08-09 19:20:00'
check_approxidate_relative 3.months.ago '2009-05-30 19:20:00'
check_approxidate_relative 2.years.3.months.ago '2007-05-30 19:20:00'

check_approxidate_relative '6am yesterday' '2009-08-29 06:00:00'
check_approxidate_relative '6pm yesterday' '2009-08-29 18:00:00'
check_approxidate_relative '3:00' '2009-08-30 03:00:00'
check_approxidate_relative '15:00' '2009-08-30 15:00:00'
check_approxidate_relative 'noon today' '2009-08-30 12:00:00'
check_approxidate_relative 'noon yesterday' '2009-08-29 12:00:00'

check_approxidate_relative 'last tuesday' '2009-08-25 19:20:00'
check_approxidate_relative 'July 5th' '2009-07-05 19:20:00'
check_approxidate_relative '06/05/2009' '2009-06-05 19:20:00'
check_approxidate_relative '06.05.2009' '2009-05-06 19:20:00'

check_approxidate_relative 'Jun 6, 5AM' '2009-06-06 05:00:00'
check_approxidate_relative '5AM Jun 6' '2009-06-06 05:00:00'
check_approxidate_relative '6AM, June 7, 2009' '2009-06-07 06:00:00'

check_approxidate_relative '2008-12-01' '2008-12-01 19:20:00'
check_approxidate_relative '2009-12-01' '2009-12-01 19:20:00'

check_approxidate_careful() {
	echo "$1 -> $2 +0000" >expect
	test_expect_${3:-success} "parse approxidate_careful ($1)" "
	test-date approxidate_careful '$1' >actual &&
	test_cmp expect actual
	"
}

check_approxidate_careful '07.04.2005.15:15:00.-0700}' '2005-04-07 22:15:00'

check_parse_to_ts() {
	echo "$1 -> $2" >expect
	test_expect_${4:-success} "parse date to ts ($1${3:+ TZ=$3})" "
	TZ=${3:-$TZ} test-date parse-to-timestamp '$1' >actual &&
	test_cmp expect actual
	"
}

check_parse_to_ts '1582-02-14 20:30:45 -0000' bad
check_parse_to_ts '1960-02-14 20:30:45 -0500' '-311725755 -300'
check_parse_to_ts '1969-12-31 23:59:59 -0000' '-1 0'
check_parse_to_ts '1970-12-31 23:59:59 -0000' '31535999 0'
check_parse_to_ts '1600-02-28 23:59:59 -0000' '-11670998401 0'
check_parse_to_ts '1600-02-29 23:59:59 -0000' '-11670912001 0'
check_parse_to_ts '1600-03-01 23:59:59 -0000' '-11670825601 0'
check_parse_to_ts '3000-03-01 23:59:59 -0000' '32508863999 0'

test_done
