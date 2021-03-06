/*
This file is part of the nppcrypt
(http://www.github.com/jeanpaulrichter/nppcrypt)
a plugin for notepad++ [ Copyright (C)2003 Don HO <don.h@free.fr> ]
(https://notepad-plus-plus.org)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef DLG_CONVERT_H_DEF
#define DLG_CONVERT_H_DEF

#include "modaldialog.h"
#include "crypt.h"

class DlgConvert : public ModalDialog
{
public:
						DlgConvert(crypt::Options::Convert& opt);

private:
	INT_PTR CALLBACK	run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam);
	void				updateOptions();
	void				enableOptions(bool v) const;
	void				OnSourceChanged(crypt::Encoding enc) const;
	void				OnTargetChanged(crypt::Encoding enc) const;

	crypt::Options::Convert&	options;
};

#endif
