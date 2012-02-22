/**
 * \file Utils.h
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Header containing some auxiliary functions declarations
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

#ifndef UTILS_H_
#define UTILS_H_

#include "typedefs.h"

namespace fbitdump {

namespace Utils {
/**
 * \brief Formats number 'num' to ostringstream 'ss'
 *
 * Uses precision 1 if output has units and 0 otherwise
 * doesn't format if 'plainNumbers' is set
 *
 * When defined as macro, it can be a bit quicker
 *
 * @param num number to format
 * @param ss string strem to put result to
 * @param plainNumbers whether to format or not
 */
template <class T>
inline void formatNumber(T num, std::ostream &ss, bool plainNumbers)
{
	ss << std::fixed;
	if (num <= 1000000 || plainNumbers) {
		ss.precision(0);
		ss << num;
	} else if (num > 1000000000) {
		ss.precision(1);
		ss << (float) num/1000000000 << " G";
	} else if (num > 1000000) {
		ss.precision(1);
		ss << (float) num/1000000 << " M";
	}
	ss.precision(0); /* set zero precision for other numbers */
}

/**
 * \brief Splits string into different tokens by comma
 *
 * @param str string to split
 * @param result stringSet to put result into
 * @return true on success, false otherwise
 */
bool splitString(char *str, stringSet &result);

} /* end of namespace utils */

}  /* end of namespace fbitdump */

#endif /* UTILS_H_ */

