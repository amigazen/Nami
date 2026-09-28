/*
 * Copyright 2017 Chris Young <chris@unsatisfactorysoftware.co.uk>
 * Copyright 2026 amigazen project
 */

#ifndef MUI_COOKIES_H
#define MUI_COOKIES_H

#include "utils/errors.h"

nserror ami_cookies_present(const char *search_term);
void ami_cookies_close(void);

#endif /* MUI_COOKIES_H */
