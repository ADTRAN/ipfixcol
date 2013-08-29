/**
 * \file Filter.h
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Header of class for filtering
 *
 * Copyright (C) 2011 CESNET, z.s.p.o.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * ALTERNATIVELY, provided that this notice is retained in full, this
 * product may be distributed under the terms of the GNU General Public
 * License (GPL) version 2 or later, in which case the provisions
 * of the GPL apply INSTEAD OF those given above.
 *
 * This software is provided ``as is, and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 *
 */

#ifndef FILTER_H_
#define FILTER_H_

#include "typedefs.h"
#include "Configuration.h"
#include "Cursor.h"
#include "parser.h"

/* both parser.y and scanner.l use this, but there is
 * no logical connection to Filter class */
#define YY_DECL int yylex(parser::Parser::semantic_type *yylval, \
    parser::Parser::location_type *yylloc, \
    fbitdump::Filter &filter, \
    yyscan_t yyscanner)

/* this is defined in scanner.h, however including it here would result
 * in inluding it back in scanner.c which is BAD */
typedef void* yyscan_t;

/**
 * \brief Namespace of the fbitdump utility
 */
namespace fbitdump {

/* Configuration and Cursor classes transitively depend on this header */
class Configuration;
class Cursor;

/**
 * \brief Class managing filter
 *
 * Parses and builds filter
 */
class Filter
{
public:

	/**
	 * \brief Print error with location
	 * @param loc location of the error
	 * @param msg error message
	 */
    void error(const parser::location &loc, const std::string &msg);

    /**
     * \brief Print error message
     * @param msg message to print
     */
    void error(const std::string &msg);


	/**
	 * \brief Constructor
	 *
	 * @param conf Configuration class
	 */
	Filter(Configuration &conf) throw (std::invalid_argument);

	/**
	 * \brief Empty Construtor
	 *
	 * Creates filter that does nothing.
	 * init() does nothing with this setup
	 */
	Filter();

	/**
	 * \brief Build and return filter string for fastbit query
	 *
	 * This should take specified time  windows into consideration
	 *
	 * @return Filter string
	 */
	const std::string getFilter() const;

	/**
	 * \brief Decides whether row specified by cursor matches the filter
	 *
	 * @param cur Cursor pointing to table row
	 * @return True when line specified by cursor matches the filter (passes)
	 */
	bool isValid(Cursor &cur) const;


	std::string parse_number(std::string number);

	std::string parse_ipv4(std::string addr);

	std::string parse_ipv6(std::string addr);

	std::string parse_timestamp(std::string timesamp);

	std::string parse_column(std::string strcol);

	yyscan_t scaninfo;	/**< lexer context */

	void set_filterString(std::string newFilter);

	void set_cmp(std::string newCmp);

private:
	/**
	 * \brief Initialise the filter
	 *
	 * Uses filter string and XML source from configuration
	 *
	 * @param conf Configuration passed to constructor
	 */
	void init(Configuration &conf) throw (std::invalid_argument);

	/**
	 * \brief Parse timestamp to number of seconds
	 *
	 * @param str String with text representation of the timestamp
	 * @return Number of seconds in timestamp
	 */
	time_t parseTimestamp(std::string str) const throw (std::invalid_argument);

	Configuration *actualConf; /**< Used configuration for getting column names while parsing filter */

	std::string secondPart; /**< Buffer for columns with more parts (IPv6...) */

	std::string filterString; /**< String for fastbit condition */

	std::string cmp;
};

}  // namespace fbitdump


#endif /* FILTER_H_ */
