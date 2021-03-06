/*
 * GIT - The information manager from hell
 *
 * Copyright (C) Linus Torvalds, 2005
 */

#include "cache.h"

static const int YEAR_MIN = 1583;  // Gregorian calendar was introduced in October 1582
static const int YEAR_MAX = 2999;
 
static int number_of_leap_days(int year)
{
	return year / 4 - year / 100 + year / 400;
}

static int is_leap_year(int year)
{
	if (year % 4)
		return 0;
	if (year % 100)
		return 1;
	if (year % 400)
		return 0;
	return 1;
}

static int is_before_leap_day_of_leap_year(int month, int year)
{
	return month < 2 && is_leap_year(year);
}

/*
 * This is like mktime, but without normalization of tm_wday and tm_yday.
 */
static int tm_to_time_t(const struct tm *tm, time_t *time)
{
	static const int mdays[] = {
	    0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
	};
	int year = tm->tm_year + 1900;
	int month = tm->tm_mon;
	int day = tm->tm_mday;

	if (year < YEAR_MIN)
		return -1;
	if (month < 0 || month > 11) /* array bounds */
		return -1;
	if (tm->tm_hour < 0 || tm->tm_min < 0 || tm->tm_sec < 0)
		return -1;

	int leap_days_of_time = number_of_leap_days(year);
	int leap_days_of_epoch = number_of_leap_days(1970);
	int leap_days_since_epoch = leap_days_of_time - leap_days_of_epoch;
	int full_days_in_month = day - 1;
	int days = (year - 1970) * 365 + leap_days_since_epoch + mdays[month] + full_days_in_month;
	if (is_before_leap_day_of_leap_year(month, year)) {
		days--;
	}

	*time = days * 24*60*60L + tm->tm_hour * 60*60 + tm->tm_min * 60 + tm->tm_sec;
	return 0;
}

static void set_time_to_0_if_time_is_invalid(struct tm *tm)
{
	if (tm->tm_hour < 0 || tm->tm_min < 0 || tm->tm_sec < 0) {
		tm->tm_hour = 0;
		tm->tm_min = 0;
		tm->tm_sec = 0;
	}
}

static int days_between(struct tm from, struct tm to)
{
	set_time_to_0_if_time_is_invalid(&from);
	set_time_to_0_if_time_is_invalid(&to);

	time_t time_t_from, time_t_to;
	tm_to_time_t(&from, &time_t_from);
	tm_to_time_t(&to, &time_t_to);

	return (time_t_to - time_t_from) / (24*60*60L);
}


static const char *month_names[] = {
	"January", "February", "March", "April", "May", "June",
	"July", "August", "September", "October", "November", "December"
};

static const char *weekday_names[] = {
	"Sundays", "Mondays", "Tuesdays", "Wednesdays", "Thursdays", "Fridays", "Saturdays"
};

static time_t gm_time_t(time_t time, int tz)
{
	int minutes;

	minutes = tz < 0 ? -tz : tz;
	minutes = (minutes / 100)*60 + (minutes % 100);
	minutes = tz < 0 ? -minutes : minutes;
	return time + minutes * 60;
}

/*
 * The "tz" thing is passed in as this strange "decimal parse of tz"
 * thing, which means that tz -0100 is passed in as the integer -100,
 * even though it means "sixty minutes off"
 */
static struct tm *time_to_tm(time_t time, int tz)
{
	time_t t = gm_time_t(time, tz);
	return gmtime(&t);
}

/*
 * What value of "tz" was in effect back then at "time" in the
 * local timezone?
 */
static int local_tzoffset(time_t time)
{
	time_t t, t_local;
	struct tm tm;
	int offset, eastwest;

	t = time;
	localtime_r(&t, &tm);
	if (tm_to_time_t(&tm, &t_local)) {
		return 0;
	}

	if (t_local < t) {
		eastwest = -1;
		offset = t - t_local;
	} else {
		eastwest = 1;
		offset = t_local - t;
	}
	offset /= 60; /* in minutes */
	offset = (offset % 60) + ((offset / 60) * 100);
	return offset * eastwest;
}

void show_date_relative(time_t time, int tz,
			       const struct timeval *now,
			       struct strbuf *timebuf)
{
	unsigned long diff;
	if (now->tv_sec < time) {
		strbuf_addstr(timebuf, _("in the future"));
		return;
	}
	diff = now->tv_sec - time;
	if (diff < 90) {
		strbuf_addf(timebuf,
			 Q_("%lu second ago", "%lu seconds ago", diff), diff);
		return;
	}
	/* Turn it into minutes */
	diff = (diff + 30) / 60;
	if (diff < 90) {
		strbuf_addf(timebuf,
			 Q_("%lu minute ago", "%lu minutes ago", diff), diff);
		return;
	}
	/* Turn it into hours */
	diff = (diff + 30) / 60;
	if (diff < 36) {
		strbuf_addf(timebuf,
			 Q_("%lu hour ago", "%lu hours ago", diff), diff);
		return;
	}
	/* We deal with number of days from here on */
	diff = (diff + 12) / 24;
	if (diff < 14) {
		strbuf_addf(timebuf,
			 Q_("%lu day ago", "%lu days ago", diff), diff);
		return;
	}
	/* Say weeks for the past 10 weeks or so */
	if (diff < 70) {
		strbuf_addf(timebuf,
			 Q_("%lu week ago", "%lu weeks ago", (diff + 3) / 7),
			 (diff + 3) / 7);
		return;
	}
	/* Say months for the past 12 months or so */
	if (diff < 365) {
		strbuf_addf(timebuf,
			 Q_("%lu month ago", "%lu months ago", (diff + 15) / 30),
			 (diff + 15) / 30);
		return;
	}
	/* Give years and months for 5 years or so */
	if (diff < 1825) {
		unsigned long totalmonths = (diff * 12 * 2 + 365) / (365 * 2);
		unsigned long years = totalmonths / 12;
		unsigned long months = totalmonths % 12;
		if (months) {
			struct strbuf sb = STRBUF_INIT;
			strbuf_addf(&sb, Q_("%lu year", "%lu years", years), years);
			strbuf_addf(timebuf,
				 /* TRANSLATORS: "%s" is "<n> years" */
				 Q_("%s, %lu month ago", "%s, %lu months ago", months),
				 sb.buf, months);
			strbuf_release(&sb);
		} else
			strbuf_addf(timebuf,
				 Q_("%lu year ago", "%lu years ago", years), years);
		return;
	}
	/* Otherwise, just years. Centuries is probably overkill. */
	strbuf_addf(timebuf,
		 Q_("%lu year ago", "%lu years ago", (diff + 183) / 365),
		 (diff + 183) / 365);
}

const char *show_date(time_t time, int tz, enum date_mode mode)
{
	struct tm *tm;
	static struct strbuf timebuf = STRBUF_INIT;

	if (mode == DATE_RAW) {
		strbuf_reset(&timebuf);
		strbuf_addf(&timebuf, "%ld %+05d", time, tz);
		return timebuf.buf;
	}

	if (mode == DATE_RELATIVE) {
		struct timeval now;

		strbuf_reset(&timebuf);
		gettimeofday(&now, NULL);
		show_date_relative(time, tz, &now, &timebuf);
		return timebuf.buf;
	}

	if (mode == DATE_LOCAL)
		tz = local_tzoffset(time);

	tm = time_to_tm(time, tz);
	if (!tm) {
		tm = time_to_tm(0, 0);
		tz = 0;
	}

	strbuf_reset(&timebuf);
	if (mode == DATE_SHORT)
		strbuf_addf(&timebuf, "%04d-%02d-%02d", tm->tm_year + 1900,
				tm->tm_mon + 1, tm->tm_mday);
	else if (mode == DATE_ISO8601)
		strbuf_addf(&timebuf, "%04d-%02d-%02d %02d:%02d:%02d %+05d",
				tm->tm_year + 1900,
				tm->tm_mon + 1,
				tm->tm_mday,
				tm->tm_hour, tm->tm_min, tm->tm_sec,
				tz);
	else if (mode == DATE_ISO8601_STRICT) {
		char sign = (tz >= 0) ? '+' : '-';
		tz = abs(tz);
		strbuf_addf(&timebuf, "%04d-%02d-%02dT%02d:%02d:%02d%c%02d:%02d",
				tm->tm_year + 1900,
				tm->tm_mon + 1,
				tm->tm_mday,
				tm->tm_hour, tm->tm_min, tm->tm_sec,
				sign, tz / 100, tz % 100);
	} else if (mode == DATE_RFC2822)
		strbuf_addf(&timebuf, "%.3s, %d %.3s %d %02d:%02d:%02d %+05d",
			weekday_names[tm->tm_wday], tm->tm_mday,
			month_names[tm->tm_mon], tm->tm_year + 1900,
			tm->tm_hour, tm->tm_min, tm->tm_sec, tz);
	else
		strbuf_addf(&timebuf, "%.3s %.3s %d %02d:%02d:%02d %d%c%+05d",
				weekday_names[tm->tm_wday],
				month_names[tm->tm_mon],
				tm->tm_mday,
				tm->tm_hour, tm->tm_min, tm->tm_sec,
				tm->tm_year + 1900,
				(mode == DATE_LOCAL) ? 0 : ' ',
				tz);
	return timebuf.buf;
}

/*
 * Check these. And note how it doesn't do the summer-time conversion.
 *
 * In my world, it's always summer, and things are probably a bit off
 * in other ways too.
 */
static const struct {
	const char *name;
	int offset;
	int dst;
} timezone_names[] = {
	{ "IDLW", -12, 0, },	/* International Date Line West */
	{ "NT",   -11, 0, },	/* Nome */
	{ "CAT",  -10, 0, },	/* Central Alaska */
	{ "HST",  -10, 0, },	/* Hawaii Standard */
	{ "HDT",  -10, 1, },	/* Hawaii Daylight */
	{ "YST",   -9, 0, },	/* Yukon Standard */
	{ "YDT",   -9, 1, },	/* Yukon Daylight */
	{ "PST",   -8, 0, },	/* Pacific Standard */
	{ "PDT",   -8, 1, },	/* Pacific Daylight */
	{ "MST",   -7, 0, },	/* Mountain Standard */
	{ "MDT",   -7, 1, },	/* Mountain Daylight */
	{ "CST",   -6, 0, },	/* Central Standard */
	{ "CDT",   -6, 1, },	/* Central Daylight */
	{ "EST",   -5, 0, },	/* Eastern Standard */
	{ "EDT",   -5, 1, },	/* Eastern Daylight */
	{ "AST",   -3, 0, },	/* Atlantic Standard */
	{ "ADT",   -3, 1, },	/* Atlantic Daylight */
	{ "WAT",   -1, 0, },	/* West Africa */

	{ "GMT",    0, 0, },	/* Greenwich Mean */
	{ "UTC",    0, 0, },	/* Universal (Coordinated) */
	{ "Z",      0, 0, },    /* Zulu, alias for UTC */

	{ "WET",    0, 0, },	/* Western European */
	{ "BST",    0, 1, },	/* British Summer */
	{ "CET",   +1, 0, },	/* Central European */
	{ "MET",   +1, 0, },	/* Middle European */
	{ "MEWT",  +1, 0, },	/* Middle European Winter */
	{ "MEST",  +1, 1, },	/* Middle European Summer */
	{ "CEST",  +1, 1, },	/* Central European Summer */
	{ "MESZ",  +1, 1, },	/* Middle European Summer */
	{ "FWT",   +1, 0, },	/* French Winter */
	{ "FST",   +1, 1, },	/* French Summer */
	{ "EET",   +2, 0, },	/* Eastern Europe, USSR Zone 1 */
	{ "EEST",  +2, 1, },	/* Eastern European Daylight */
	{ "WAST",  +7, 0, },	/* West Australian Standard */
	{ "WADT",  +7, 1, },	/* West Australian Daylight */
	{ "CCT",   +8, 0, },	/* China Coast, USSR Zone 7 */
	{ "JST",   +9, 0, },	/* Japan Standard, USSR Zone 8 */
	{ "EAST", +10, 0, },	/* Eastern Australian Standard */
	{ "EADT", +10, 1, },	/* Eastern Australian Daylight */
	{ "GST",  +10, 0, },	/* Guam Standard, USSR Zone 9 */
	{ "NZT",  +12, 0, },	/* New Zealand */
	{ "NZST", +12, 0, },	/* New Zealand Standard */
	{ "NZDT", +12, 1, },	/* New Zealand Daylight */
	{ "IDLE", +12, 0, },	/* International Date Line East */
};

static int match_string(const char *date, const char *str)
{
	int i = 0;

	for (i = 0; *date; date++, str++, i++) {
		if (*date == *str)
			continue;
		if (toupper(*date) == toupper(*str))
			continue;
		if (!isalnum(*date))
			break;
		return 0;
	}
	return i;
}

/*
* Parse month, weekday, or timezone name
*/
static int match_alpha(const char *date, struct tm *tm, int *offset, int *parsed_chars)
{
	if (!isalpha(date[0]))
		return -1;

	int i;

	for (i = 0; i < 12; i++) {
		int match = match_string(date, month_names[i]);
		if (match >= 3) {
			tm->tm_mon = i;
			*parsed_chars = match;
			return 0;			
		}
	}

	for (i = 0; i < 7; i++) {
		int match = match_string(date, weekday_names[i]);
		if (match >= 3) {
			tm->tm_wday = i;
			*parsed_chars = match;
			return 0;
		}
	}

	for (i = 0; i < ARRAY_SIZE(timezone_names); i++) {
		int match = match_string(date, timezone_names[i].name);
		if (match >= 3 || match == strlen(timezone_names[i].name)) {
			int off = timezone_names[i].offset;

			/* This is bogus, but we like summer */
			off += timezone_names[i].dst;

			/* Only use the tz name offset if we don't have anything better */
			if (*offset == -1)
				*offset = 60*off;

			*parsed_chars = match;
			return 0;
		}
	}

	if (match_string(date, "PM") == 2) {
		tm->tm_hour = (tm->tm_hour % 12) + 12;
		*parsed_chars = 2;
		return 0;
	}

	if (match_string(date, "AM") == 2) {
		tm->tm_hour = (tm->tm_hour % 12) + 0;
		*parsed_chars = 2;
		return 0;
	}

	return -1;
}

enum DateFormat { INVALID=0, YYYY_MM_DD, YYYY_DD_MM, DD_MM_YYYY, MM_DD_YYYY };



static int could_be_a_year(int year) {
	return (YEAR_MIN <= year && year <= YEAR_MAX)
		|| (70 < year && year <= 99)
		|| (0 <= year && year < 38);
}

static int could_be_a_month(int month) {
	return 0 < month && month < 13;
}

static int could_be_a_day(int day) {
	return 0 < day && day < 32;
}

static int could_be_a_hour(int hour) {
	/* do we really need <= 24? */
	return 0 <= hour && hour <= 24;
}

static int could_be_a_minute(int min) {
	return 0 <= min && min < 60;
}

static int could_be_a_second(int sec) {
	return 0 <= sec && sec <= 60;
}

static int normalized_month_for_tm(int month) {
	return month - 1;
}

static int normalized_year(int year) {
	if (70 < year && year <= 99)
		return year + 1900;
	else if (0 <= year && year < 38)
		return year + 2000;

	return year;
}

static int normalized_year_for_tm(int year) {
	return normalized_year(year) - 1900;
}

static void fill_date_in_tm(int year, int month, int day, struct tm *tm)
{
	tm->tm_year = normalized_year_for_tm(year);
	tm->tm_mon = normalized_month_for_tm(month);
	tm->tm_mday = day;
}

static void fill_time_in_tm(int hour, int min, int sec, struct tm *tm)
{
	tm->tm_hour = hour;
	tm->tm_min = min;
	tm->tm_sec = sec;
}

static int init_and_fill_tm(int num1, int num2, int num3, enum DateFormat order, struct tm *tm)
{
	int year, month, day;
	memset(tm, 0, sizeof(*tm));
	tm->tm_isdst = -1;

	switch (order) {
		case YYYY_DD_MM:
			year = num1;
			day = num2;
			month = num3;
			break;
		case YYYY_MM_DD:
			year = num1;
			month = num2;
			day = num3;
			break;
		case DD_MM_YYYY:
			day = num1;
			month = num2;
			year = num3;
			break;
		case MM_DD_YYYY:
			month = num1;
			day = num2;
			year = num3;
			break;
		default:
			return 0;
	}

	fill_date_in_tm(year, month, day, tm);
	return 1;
}

static enum DateFormat get_date_order(int num1, int num2, int num3, char separator, struct tm *now_tm)
{
	struct tm tm_tmp;

	if (num1 > 70) {
		/* yyyy-mm-dd? */
		if (could_be_a_month(num2) && could_be_a_day(num3))
			return YYYY_MM_DD;
		/* yyyy-dd-mm? */
		else if (could_be_a_day(num2) && could_be_a_month(num3))
			return YYYY_DD_MM;
	}

	/* Our eastern European friends say dd.mm.yy[yy]
	 * is the norm there, so giving precedence to
	 * mm/dd/yy[yy] form only when separator is not '.'
	 */
	if (separator != '.' &&
		could_be_a_month(num1) && could_be_a_day(num2) && could_be_a_year(num3)) {
		init_and_fill_tm(num1, num2, num3, MM_DD_YYYY, &tm_tmp);
		if (days_between(*now_tm, tm_tmp) <= 10)
			return MM_DD_YYYY;
	}

	/* European dd.mm.yy[yy] or funny US dd/mm/yy[yy] */
	if (could_be_a_day(num1) && could_be_a_month(num2) && could_be_a_year(num3)) {
		init_and_fill_tm(num1, num2, num3, DD_MM_YYYY, &tm_tmp);
		if (days_between(*now_tm, tm_tmp) <= 10)
			return DD_MM_YYYY;
	}

	/* Funny European mm.dd.yy */
	if (separator == '.' &&
		could_be_a_month(num1) && could_be_a_day(num2) && could_be_a_year(num3)) {
		init_and_fill_tm(num1, num2, num3, MM_DD_YYYY, &tm_tmp);
		if (days_between(*now_tm, tm_tmp) <= 10)
			return MM_DD_YYYY;
	}

	return INVALID;
}

static int match_multi_number(unsigned long num, char c, const char *date,
			      char *end, struct tm *tm, time_t now)
{
	struct tm now_tm;
	struct tm *refuse_future;
	long num2, num3;

	num2 = strtol(end+1, &end, 10);
	num3 = -1;
	if (*end == c && isdigit(end[1]))
		num3 = strtol(end+1, &end, 10);

	/* Time? Date? */
	switch (c) {
	case ':':
		if (num3 < 0)
			num3 = 0;
		if (could_be_a_hour(num) &&
		    could_be_a_minute(num2) &&
		    could_be_a_second(num3)) {
			fill_time_in_tm(num, num2, num3, tm);
			break;
		}
		return 0;

	case '-':
	case '/':
	case '.':
		if (!now)
			now = time(NULL);
		refuse_future = NULL;
		if (gmtime_r(&now, &now_tm))
			refuse_future = &now_tm;

		enum DateFormat format = get_date_order(num, num2, num3, c, refuse_future);
		switch (format) {
		case YYYY_MM_DD:
			fill_date_in_tm(num, num2, num3, tm);
			break;
		case YYYY_DD_MM:
			fill_date_in_tm(num, num3, num2, tm);
			break;
		case DD_MM_YYYY:
			fill_date_in_tm(num3, num2, num, tm);
			break;
		case MM_DD_YYYY:
			fill_date_in_tm(num3, num, num2, tm);
			break;
		default:
			return 0;
		}
		break;
	}
	return end - date;
}

/*
 * Have we filled in any part of the time/date yet?
 * We just do a binary 'and' to see if the sign bit
 * is set in all the values.
 */
static inline int nodate(struct tm *tm)
{
	return (tm->tm_year &
		tm->tm_mon &
		tm->tm_mday &
		tm->tm_hour &
		tm->tm_min &
		tm->tm_sec) < 0;
}

/*
 * We've seen a digit. Time? Year? Date?
 */
static int match_digit(const char *date, struct tm *tm, int *offset, int *tm_gmt, int *parsed_chars)
{
	if (!((isdigit(date[0]) || date[0] == '-' || date[0] == '+') && isdigit(date[1])))
		return -1;

	int n;
	char *end;
	long num;

	num = strtol(date, &end, 10);

	/*
	 * Seconds since 1970? We trigger on that for any numbers with
	 * more than 8 digits. This is because we don't want to rule out
	 * numbers like 20070606 as a YYYYMMDD date.
	 */
	if ((num >= 100000000 || num <= -100000000)  && nodate(tm)) {
		time_t time = num;
		if (gmtime_r(&time, tm)) {
			*tm_gmt = 1;
			*parsed_chars = end - date;
			return 0;
		}
	}

	/*
	 * Numbers starting with a sign has to represent an epoch value,
	 * which is parsed above.
	 */
	if (date[0] == '-' || date[1] == '+')
		return -1;

	/*
	 * Check for special formats: num[-.:/]num[same]num
	 */
	switch (*end) {
	case ':':
	case '.':
	case '/':
	case '-':
		if (isdigit(end[1])) {
			int match = match_multi_number(num, *end, date, end, tm, 0);
			if (match) {
				*parsed_chars = match;
				return 0;
			}
		}
	}

	/*
	 * None of the special formats? Try to guess what
	 * the number meant. We use the number of digits
	 * to make a more educated guess..
	 */
	n = 0;
	do {
		n++;
	} while (isdigit(date[n]));

	/* Four-digit year or a timezone? */
	if (n == 4) {
		if (num <= 1400 && *offset == -1) {
			unsigned int minutes = num % 100;
			unsigned int hours = num / 100;
			*offset = hours*60 + minutes;
		} else if (num > 1900 && num < 2100)
			tm->tm_year = num - 1900;
		*parsed_chars = n;
		return 0;
	}

	/*
	 * Ignore lots of numerals. We took care of 4-digit years above.
	 * Days or months must be one or two digits.
	 */
	if (n > 2) {
		*parsed_chars = n;
		return 0;
	}

	/*
	 * NOTE! We will give precedence to day-of-month over month or
	 * year numbers in the 1-12 range. So 05 is always "mday 5",
	 * unless we already have a mday..
	 *
	 * IOW, 01 Apr 05 parses as "April 1st, 2005".
	 */
	if (num > 0 && num < 32 && tm->tm_mday < 0) {
		tm->tm_mday = num;
		*parsed_chars = n;
		return 0;
	}

	/* Two-digit year? */
	if (n == 2 && tm->tm_year < 0) {
		if (num < 10 && tm->tm_mday >= 0) {
			tm->tm_year = num + 100;
			*parsed_chars = n;
			return 0;
		}
		if (num >= 70) {
			tm->tm_year = num;
			*parsed_chars = n;
			return 0;
		}
	}

	if (num > 0 && num < 13 && tm->tm_mon < 0)
		tm->tm_mon = num-1;

	*parsed_chars = n;
	return 0;
}

static int match_tz(const char *date, int *offp, int *parsed_chars)
{
	if (!((date[0] == '-' || date[0] == '+') && isdigit(date[1])))
		return -1;

	char *end;
	int hour = strtoul(date + 1, &end, 10);
	int n = end - (date + 1);
	int min = 0;

	if (n == 4) {
		/* hhmm */
		min = hour % 100;
		hour = hour / 100;
	} else if (n != 2) {
		min = 99; /* random crap */
	} else if (*end == ':') {
		/* hh:mm? */
		min = strtoul(end + 1, &end, 10);
		if (end - (date + 1) != 5)
			min = 99; /* random crap */
	} /* otherwise we parsed "hh" */

	/*
	 * Don't accept any random crap. Even though some places have
	 * offset larger than 12 hours (e.g. Pacific/Kiritimati is at
	 * UTC+14), there is something wrong if hour part is much
	 * larger than that. We might also want to check that the
	 * minutes are divisible by 15 or something too. (Offset of
	 * Kathmandu, Nepal is UTC+5:45)
	 */
	if (min < 60 && hour < 24) {
		int offset = hour * 60 + min;
		if (*date == '-')
			offset = -offset;
		*offp = offset;
		*parsed_chars = end - date;
		return 0;
	} else {
		*parsed_chars = 0;
		return -1;
	}
}

static void date_string(time_t date, int offset, struct strbuf *buf)
{
	int sign = '+';

	if (offset < 0) {
		offset = -offset;
		sign = '-';
	}
	strbuf_addf(buf, "%ld %c%02d%02d", date, sign, offset / 60, offset % 60);
}

/*
 * Parse a string like "0 +0000" as ancient timestamp near epoch, but
 * only when it appears not as part of any other string.
 */
static int match_object_header_date(const char *date, time_t *timestamp, int *offset)
{
	char *end;
	long stamp;
	int ofs;

	if ((*date < '0' || '9' < *date) && !(*date == '+' || *date == '-'))
		return -1;
	stamp = strtol(date, &end, 10);
	if (*end != ' ' || stamp == LONG_MAX || (end[1] != '+' && end[1] != '-'))
		return -1;
	date = end + 2;
	ofs = strtol(date, &end, 10);
	if ((*end != '\0' && (*end != '\n')) || end != date + 4)
		return -1;
	ofs = (ofs / 100) * 60 + (ofs % 100);
	if (date[-1] == '-')
		ofs = -ofs;
	*timestamp = stamp;
	*offset = ofs;
	return 0;
}

/* Gr. strptime is crap for this; it doesn't have a way to require RFC2822
   (i.e. English) day/month names, and it doesn't work correctly with %z. */
int parse_date_basic(const char *date, time_t *timestamp, int *offset)
{
	struct tm tm;
	int tm_gmt;
	time_t dummy_timestamp;
	int dummy_offset;

	if (!timestamp)
		timestamp = &dummy_timestamp;
	if (!offset)
		offset = &dummy_offset;

	memset(&tm, 0, sizeof(tm));
	tm.tm_year = -1;
	tm.tm_mon = -1;
	tm.tm_mday = -1;
	tm.tm_isdst = -1;
	tm.tm_hour = -1;
	tm.tm_min = -1;
	tm.tm_sec = -1;
	*offset = -1;
	tm_gmt = 0;

	if (*date == '@' &&
	    !match_object_header_date(date + 1, timestamp, offset))
		return 0; /* success */
	for (;;) {
		int parsed_chars = 0;
		unsigned char c = *date;

		/* Stop at end of string or newline */
		if (!c || c == '\n')
			break;

		if (match_alpha(date, &tm, offset, &parsed_chars) &&
			match_tz(date, offset, &parsed_chars) &&
			match_digit(date, &tm, offset, &tm_gmt, &parsed_chars)) {
			parsed_chars = 1;
		}

		date += parsed_chars;
	}

	/* do not use mktime(), which uses local timezone, here */
	if (tm_to_time_t(&tm, timestamp))
		return -1;

	if (*offset == -1) {
		time_t temp_time;

		/* gmtime_r() in match_digit() may have clobbered it */
		tm.tm_isdst = -1;
		temp_time = mktime(&tm);
		if ((time_t)*timestamp > temp_time) {
			*offset = ((time_t)*timestamp - temp_time) / 60;
		} else {
			*offset = -(int)((temp_time - (time_t)*timestamp) / 60);
		}
	}

	if (!tm_gmt)
		*timestamp -= *offset * 60;
	return 0; /* success */
}

int parse_expiry_date(const char *date, time_t *timestamp)
{
	int errors = 0;

	if (!strcmp(date, "never") || !strcmp(date, "false"))
		*timestamp = 0;
	else if (!strcmp(date, "all") || !strcmp(date, "now"))
		/*
		 * We take over "now" here, which usually translates
		 * to the current timestamp.  This is because the user
		 * really means to expire everything he or she has done in
		 * the past, and by definition reflogs are the record
		 * of the past, and there is nothing from the future
		 * to be kept.
		 */
		*timestamp = LONG_MAX;
	else
		*timestamp = approxidate_careful(date, &errors);

	return errors;
}

int parse_date(const char *date, struct strbuf *result)
{
	time_t timestamp;
	int offset;
	int ret = parse_date_basic(date, &timestamp, &offset);
	if (ret)
		return ret;
	date_string(timestamp, offset, result);
	return 0;
}

enum date_mode parse_date_format(const char *format)
{
	if (!strcmp(format, "relative"))
		return DATE_RELATIVE;
	else if (!strcmp(format, "iso8601") ||
		 !strcmp(format, "iso"))
		return DATE_ISO8601;
	else if (!strcmp(format, "iso8601-strict") ||
		 !strcmp(format, "iso-strict"))
		return DATE_ISO8601_STRICT;
	else if (!strcmp(format, "rfc2822") ||
		 !strcmp(format, "rfc"))
		return DATE_RFC2822;
	else if (!strcmp(format, "short"))
		return DATE_SHORT;
	else if (!strcmp(format, "local"))
		return DATE_LOCAL;
	else if (!strcmp(format, "default"))
		return DATE_NORMAL;
	else if (!strcmp(format, "raw"))
		return DATE_RAW;
	else
		die("unknown date format %s", format);
}

void datestamp(struct strbuf *out)
{
	time_t now;
	time_t offset = -1;

	time(&now);

	int ret = tm_to_time_t(localtime(&now), &offset);
	if (ret)
		offset = -1;
	offset -= now;
	offset /= 60;

	date_string(now, offset, out);
}

/*
 * Relative time update (eg "2 days ago").  If we haven't set the time
 * yet, we need to set it from current time.
 */
static time_t update_tm(struct tm *tm, struct tm *now, unsigned long sec)
{
	time_t n;

	if (tm->tm_mday < 0)
		tm->tm_mday = now->tm_mday;
	if (tm->tm_mon < 0)
		tm->tm_mon = now->tm_mon;
	if (tm->tm_year < 0) {
		tm->tm_year = now->tm_year;
		if (tm->tm_mon > now->tm_mon)
			tm->tm_year--;
	}

	n = mktime(tm) - sec;
	localtime_r(&n, tm);
	return n;
}

static void date_now(struct tm *tm, struct tm *now, int *num)
{
	update_tm(tm, now, 0);
}

static void date_yesterday(struct tm *tm, struct tm *now, int *num)
{
	update_tm(tm, now, 24*60*60);
}

static void date_time(struct tm *tm, struct tm *now, int hour)
{
	if (tm->tm_hour < hour)
		date_yesterday(tm, now, NULL);
	tm->tm_hour = hour;
	tm->tm_min = 0;
	tm->tm_sec = 0;
}

static void date_midnight(struct tm *tm, struct tm *now, int *num)
{
	date_time(tm, now, 0);
}

static void date_noon(struct tm *tm, struct tm *now, int *num)
{
	date_time(tm, now, 12);
}

static void date_tea(struct tm *tm, struct tm *now, int *num)
{
	date_time(tm, now, 17);
}

static void date_pm(struct tm *tm, struct tm *now, int *num)
{
	int hour, n = *num;
	*num = 0;

	hour = tm->tm_hour;
	if (n) {
		hour = n;
		tm->tm_min = 0;
		tm->tm_sec = 0;
	}
	tm->tm_hour = (hour % 12) + 12;
}

static void date_am(struct tm *tm, struct tm *now, int *num)
{
	int hour, n = *num;
	*num = 0;

	hour = tm->tm_hour;
	if (n) {
		hour = n;
		tm->tm_min = 0;
		tm->tm_sec = 0;
	}
	tm->tm_hour = (hour % 12);
}

static void date_never(struct tm *tm, struct tm *now, int *num)
{
	time_t n = 0;
	localtime_r(&n, tm);
}

static const struct special {
	const char *name;
	void (*fn)(struct tm *, struct tm *, int *);
} special[] = {
	{ "yesterday", date_yesterday },
	{ "noon", date_noon },
	{ "midnight", date_midnight },
	{ "tea", date_tea },
	{ "PM", date_pm },
	{ "AM", date_am },
	{ "never", date_never },
	{ "now", date_now },
	{ NULL }
};

static const char *number_name[] = {
	"zero", "one", "two", "three", "four",
	"five", "six", "seven", "eight", "nine", "ten",
};

static const struct typelen {
	const char *type;
	int length;
} typelen[] = {
	{ "seconds", 1 },
	{ "minutes", 60 },
	{ "hours", 60*60 },
	{ "days", 24*60*60 },
	{ "weeks", 7*24*60*60 },
	{ NULL }
};

static const char *approxidate_alpha(const char *date, struct tm *tm, struct tm *now, int *num, int *touched)
{
	const struct typelen *tl;
	const struct special *s;
	const char *end = date;
	int i;

	while (isalpha(*++end))
		;

	for (i = 0; i < 12; i++) {
		int match = match_string(date, month_names[i]);
		if (match >= 3) {
			tm->tm_mon = i;
			*touched = 1;
			return end;
		}
	}

	for (s = special; s->name; s++) {
		int len = strlen(s->name);
		if (match_string(date, s->name) == len) {
			s->fn(tm, now, num);
			*touched = 1;
			return end;
		}
	}

	if (!*num) {
		for (i = 1; i < 11; i++) {
			int len = strlen(number_name[i]);
			if (match_string(date, number_name[i]) == len) {
				*num = i;
				*touched = 1;
				return end;
			}
		}
		if (match_string(date, "last") == 4) {
			*num = 1;
			*touched = 1;
		}
		return end;
	}

	tl = typelen;
	while (tl->type) {
		int len = strlen(tl->type);
		if (match_string(date, tl->type) >= len-1) {
			update_tm(tm, now, tl->length * *num);
			*num = 0;
			*touched = 1;
			return end;
		}
		tl++;
	}

	for (i = 0; i < 7; i++) {
		int match = match_string(date, weekday_names[i]);
		if (match >= 3) {
			int diff, n = *num -1;
			*num = 0;

			diff = tm->tm_wday - i;
			if (diff <= 0)
				n++;
			diff += 7*n;

			update_tm(tm, now, diff * 24 * 60 * 60);
			*touched = 1;
			return end;
		}
	}

	if (match_string(date, "months") >= 5) {
		int n;
		update_tm(tm, now, 0); /* fill in date fields if needed */
		n = tm->tm_mon - *num;
		*num = 0;
		while (n < 0) {
			n += 12;
			tm->tm_year--;
		}
		tm->tm_mon = n;
		*touched = 1;
		return end;
	}

	if (match_string(date, "years") >= 4) {
		update_tm(tm, now, 0); /* fill in date fields if needed */
		tm->tm_year -= *num;
		*num = 0;
		*touched = 1;
		return end;
	}

	return end;
}

static const char *approxidate_digit(const char *date, struct tm *tm, int *num,
				     time_t now)
{
	char *end;
	unsigned long number = strtoul(date, &end, 10);

	switch (*end) {
	case ':':
	case '.':
	case '/':
	case '-':
		if (isdigit(end[1])) {
			int match = match_multi_number(number, *end, date, end,
						       tm, now);
			if (match)
				return date + match;
		}
	}

	/* Accept zero-padding only for small numbers ("Dec 02", never "Dec 0002") */
	if (date[0] != '0' || end - date <= 2)
		*num = number;
	return end;
}

/*
 * Do we have a pending number at the end, or when
 * we see a new one? Let's assume it's a month day,
 * as in "Dec 6, 1992"
 */
static void pending_number(struct tm *tm, int *num)
{
	int number = *num;

	if (number) {
		*num = 0;
		if (tm->tm_mday < 0 && number < 32)
			tm->tm_mday = number;
		else if (tm->tm_mon < 0 && number < 13)
			tm->tm_mon = number-1;
		else if (tm->tm_year < 0) {
			if (number > YEAR_MIN && number < YEAR_MAX)
				tm->tm_year = number - 1900;
			else if (number > 69 && number < 100)
				tm->tm_year = number;
			else if (number < 38)
				tm->tm_year = 100 + number;
			/* We screw up for number = 00 ? */
		}
	}
}

static time_t approxidate_str(const char *date,
				     const struct timeval *tv,
				     int *error_ret)
{
	int number = 0;
	int touched = 0;
	struct tm tm, now;
	time_t time_sec;

	time_sec = tv->tv_sec;
	localtime_r(&time_sec, &tm);
	now = tm;

	tm.tm_year = -1;
	tm.tm_mon = -1;
	tm.tm_mday = -1;

	for (;;) {
		unsigned char c = *date;
		if (!c)
			break;
		date++;
		if (isdigit(c)) {
			pending_number(&tm, &number);
			date = approxidate_digit(date-1, &tm, &number, time_sec);
			touched = 1;
			continue;
		}
		if (isalpha(c))
			date = approxidate_alpha(date-1, &tm, &now, &number, &touched);
	}
	pending_number(&tm, &number);
	if (!touched)
		*error_ret = 1;
	return update_tm(&tm, &now, 0);
}

time_t approxidate_relative(const char *date, const struct timeval *tv)
{
	time_t timestamp;
	int offset;
	int errors = 0;

	if (!parse_date_basic(date, &timestamp, &offset))
		return timestamp;
	return approxidate_str(date, tv, &errors);
}

time_t approxidate_careful(const char *date, int *error_ret)
{
	struct timeval tv;
	time_t timestamp;
	int offset;
	int dummy = 0;
	if (!error_ret)
		error_ret = &dummy;

	if (!parse_date_basic(date, &timestamp, &offset)) {
		*error_ret = 0;
		return timestamp;
	}

	gettimeofday(&tv, NULL);
	return approxidate_str(date, &tv, error_ret);
}

int date_overflows(unsigned long t)
{
	time_t sys;

	/* If we overflowed our unsigned long, that's bad... */
	if (t == ULONG_MAX)
		return 1;

	/*
	 * ...but we also are going to feed the result to system
	 * functions that expect time_t, which is often "signed long".
	 * Make sure that we fit into time_t, as well.
	 */
	sys = t;
	return t != sys || (t < 1) != (sys < 1);
}
