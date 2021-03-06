/*
This file is part of nppcrypt
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

#include "dlg_crypt.h"
#include "resource.h"
#include "help.h"
#include "exception.h"
#include "crypt_help.h"
#include <sstream>

DlgCrypt::DlgCrypt() : ModalDialog()
{
};

void DlgCrypt::destroy()
{
	if (tab.basic) {
		::DestroyWindow(tab.basic);
	}
	if (tab.auth) {
		::DestroyWindow(tab.auth);
	}
	if (tab.key) {
		::DestroyWindow(tab.key);
	}
	if (tab.iv) {
		::DestroyWindow(tab.iv);
	}
	if (tab.encoding) {
		::DestroyWindow(tab.encoding);
	}
	if (invalid.brush) {
		DeleteObject(invalid.brush);
	}
	tab.basic = tab.encoding = tab.key = tab.iv = tab.auth = NULL;
	ModalDialog::destroy();
};

bool DlgCrypt::doDialog(Operation operation, CryptInfo* crypt, crypt::UserData* iv, const std::wstring* filename)
{
	if (!crypt || !iv) {
		return false;
	}
	if (!invalid.brush) {
		invalid.brush = CreateSolidBrush(RGB(255, 0, 0));
	}
	this->crypt = crypt;
	this->ivdata = iv;
	this->operation = operation;
	this->filename = filename;
	confirm_password = false;
	current.iv_length = 0;
	current.tab = -1;
	invalid.iv = false;
	invalid.hmac_key = false;
	invalid.password = false;
	return ModalDialog::doDialog();
}

INT_PTR CALLBACK DlgCrypt::run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
	{
		setupDialog();
		goToCenter();
		return TRUE;
	}
	case WM_COMMAND:
	{
		switch (HIWORD(wParam))
		{
		case BN_CLICKED:
		{
			switch (LOWORD(wParam))
			{
			case IDC_OK:
			{
				if (OnClickOK()) {
					EndDialog(_hSelf, IDC_OK);
					_hSelf = NULL;
					return TRUE;
				}
				break;
			}
			case IDC_CANCEL: case IDCANCEL:
			{
				if (confirm_password) {
					confirm_password = false;
					::EnableWindow(::GetDlgItem(tab.basic, IDC_CRYPT_PASSWORD), true);
					::EnableWindow(::GetDlgItem(tab.basic, IDC_CRYPT_PASSWORD_ENC), true);
					::SetDlgItemText(tab.basic, IDC_CRYPT_PASSWORD_STATIC, TEXT("password:"));
					::SetDlgItemText(tab.basic, IDC_CRYPT_PASSWORD, TEXT(""));
					::SetFocus(::GetDlgItem(tab.basic, IDC_CRYPT_PASSWORD));
					PostMessage(tab.basic, WM_NEXTDLGCTL, (WPARAM)::GetDlgItem(tab.basic, IDC_CRYPT_PASSWORD), TRUE);
				} else {
					EndDialog(_hSelf, IDC_CANCEL);
					_hSelf = NULL;
					return TRUE;
				}
				break;
			}
			case IDC_CRYPT_ENC_ASCII:
			{
				updateEncodingControls(crypt::Encoding::ascii);
				break;
			}
			case IDC_CRYPT_ENC_BASE16:
			{
				updateEncodingControls(crypt::Encoding::base16);
				break;
			}
			case IDC_CRYPT_ENC_BASE32:
			{
				updateEncodingControls(crypt::Encoding::base32);
				break;
			}
			case IDC_CRYPT_ENC_BASE64:
			{
				updateEncodingControls(crypt::Encoding::base64);
				break;
			}
			case IDC_CRYPT_ENC_LINEBREAK:
			{
				bool linebreaks = !!::SendDlgItemMessage(tab.encoding, IDC_CRYPT_ENC_LINEBREAK, BM_GETCHECK, 0, 0);
				::EnableWindow(::GetDlgItem(tab.encoding, IDC_CRYPT_ENC_LB_WIN), linebreaks);
				::EnableWindow(::GetDlgItem(tab.encoding, IDC_CRYPT_ENC_LB_UNIX), linebreaks);
				::EnableWindow(::GetDlgItem(tab.encoding, IDC_CRYPT_ENC_LINELEN), linebreaks);
				::EnableWindow(::GetDlgItem(tab.encoding, IDC_CRYPT_ENC_LINELEN_SPIN), linebreaks);
				::EnableWindow(::GetDlgItem(tab.encoding, IDC_CRYPT_ENC_STATIC), linebreaks);
				break;
			}
			case IDC_CRYPT_KEY_PBKDF2: case IDC_CRYPT_KEY_BCRYPT: case IDC_CRYPT_KEY_SCRYPT:
			{
				if (!!::SendDlgItemMessage(tab.key, IDC_CRYPT_KEY_PBKDF2, BM_GETCHECK, 0, 0)) {
					current.key_derivation = crypt::KeyDerivation::pbkdf2;
				} else if (!!::SendDlgItemMessage(tab.key, IDC_CRYPT_KEY_BCRYPT, BM_GETCHECK, 0, 0)) {
					current.key_derivation = crypt::KeyDerivation::bcrypt;
				} else {
					current.key_derivation = crypt::KeyDerivation::scrypt;
				}
				updateKeyDerivationControls();
				break;
			}
			case IDC_CRYPT_SALT:
			{
				if (current.key_derivation != crypt::KeyDerivation::bcrypt) {
					bool c = ::SendDlgItemMessage(tab.key, IDC_CRYPT_SALT, BM_GETCHECK, 0, 0) ? true : false;
					::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_SALT_BYTES), c);
					::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_SALT_SPIN), c);
					::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_SALT_STATIC), c);
				}
				break;
			}
			case IDC_CRYPT_AUTH_ENABLE:
			{
				if (operation == Operation::Enc) {
					bool c = ::SendDlgItemMessage(tab.auth, IDC_CRYPT_AUTH_ENABLE, BM_GETCHECK, 0, 0) ? true : false;
					::EnableWindow(::GetDlgItem(tab.auth, IDC_CRYPT_AUTH_HASH), c);
					::EnableWindow(::GetDlgItem(tab.auth, IDC_CRYPT_AUTH_HASH_LENGTH), c);
					::EnableWindow(::GetDlgItem(tab.auth, IDC_CRYPT_AUTH_KEY_PRESET), c);
					::EnableWindow(::GetDlgItem(tab.auth, IDC_CRYPT_AUTH_KEY_CUSTOM), c);
					::EnableWindow(::GetDlgItem(tab.auth, IDC_CRYPT_AUTH_KEY_LIST), c);
					::EnableWindow(::GetDlgItem(tab.auth, IDC_CRYPT_AUTH_PW_VALUE), c);
					::EnableWindow(::GetDlgItem(tab.auth, IDC_CRYPT_AUTH_PW_SHOW), c);
					::EnableWindow(::GetDlgItem(tab.auth, IDC_CRYPT_AUTH_PW_ENC), c);
					::EnableWindow(::GetDlgItem(tab.auth, IDC_CRYPT_AUTH_STATIC1), c);
					if (c && ::SendDlgItemMessage(tab.auth, IDC_CRYPT_AUTH_KEY_CUSTOM, BM_GETCHECK, 0, 0)) {
						::SetFocus(::GetDlgItem(tab.auth, IDC_CRYPT_AUTH_PW_VALUE));
					}
				}
				break;
			}
			case IDC_CRYPT_AUTH_KEY_CUSTOM:
			{
				SendMessage(tab.auth, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(tab.auth, IDC_CRYPT_AUTH_PW_VALUE), TRUE);
				break;
			}
			case IDC_CRYPT_AUTH_PW_SHOW:
			{
				char c = ::SendDlgItemMessage(tab.auth, IDC_CRYPT_AUTH_PW_SHOW, BM_GETCHECK, 0, 0) ? 0 : '*';
				::SendDlgItemMessage(tab.auth, IDC_CRYPT_AUTH_PW_VALUE, EM_SETPASSWORDCHAR, c, 0);
				InvalidateRect(::GetDlgItem(tab.auth, IDC_CRYPT_AUTH_PW_VALUE), 0, TRUE);
				if (::SendDlgItemMessage(tab.auth, IDC_CRYPT_AUTH_KEY_CUSTOM, BM_GETCHECK, 0, 0)) {
					::SetFocus(::GetDlgItem(tab.auth, IDC_CRYPT_AUTH_PW_VALUE));
				}
				break;
			}
			case IDC_CRYPT_IV_CUSTOM:
			{
				updateIVControls();
				::SetFocus(::GetDlgItem(tab.iv, IDC_CRYPT_IV_INPUT));
				break;
			}
			case IDC_CRYPT_IV_RANDOM: case IDC_CRYPT_IV_KEY: case IDC_CRYPT_IV_ZERO:
			{
				updateIVControls();
				break;
			}
			case IDC_CRYPT_PASSWORD_SHOW:
			{
				char c = ::SendDlgItemMessage(tab.basic, IDC_CRYPT_PASSWORD_SHOW, BM_GETCHECK, 0, 0) ? 0 : '*';
				::SendDlgItemMessage(tab.basic, IDC_CRYPT_PASSWORD, EM_SETPASSWORDCHAR, c, 0);
				InvalidateRect(::GetDlgItem(tab.basic, IDC_CRYPT_PASSWORD), 0, TRUE);
				::SetFocus(::GetDlgItem(tab.basic, IDC_CRYPT_PASSWORD));
				break;
			}
			}
			break;
		}
		case CBN_SELCHANGE:
		{
			switch (LOWORD(wParam))
			{
			case IDC_CRYPT_CIPHER_TYPE:
			{
				int category = (int)::SendDlgItemMessage(tab.basic, IDC_CRYPT_CIPHER_TYPE, CB_GETCURSEL, 0, 0);
				::SendDlgItemMessage(tab.basic, IDC_CRYPT_CIPHER, CB_RESETCONTENT, 0, 0);
				for (crypt::help::CipherNames it(category); *it; ++it) {
					::SendDlgItemMessage(tab.basic, IDC_CRYPT_CIPHER, CB_ADDSTRING, 0, (LPARAM)helper::Windows::ToWCHAR(*it).c_str());
				}
				::SendDlgItemMessage(tab.basic, IDC_CRYPT_CIPHER, CB_SETCURSEL, 0, 0);
				updateCipherControls();
				break;
			}
			case IDC_CRYPT_CIPHER:
			{
				updateCipherControls();
				break;
			}
			case IDC_CRYPT_MODE:
			{
				crypt::Mode tmode = crypt::help::getModeByIndex(current.cipher, (int)::SendDlgItemMessage(tab.basic, IDC_CRYPT_MODE, CB_GETCURSEL, 0, 0));
				updateCipherModeInfo(tmode);
				PostMessage(tab.basic, WM_NEXTDLGCTL, (WPARAM)::GetDlgItem(tab.basic, IDC_CRYPT_PASSWORD), TRUE);
				break;
			}
			case IDC_CRYPT_PASSWORD_ENC:
			{
				crypt::secure_string temp;
				checkPassword(temp, false);
				::SetFocus(::GetDlgItem(tab.basic, IDC_CRYPT_PASSWORD));
				break;
			}
			case IDC_CRYPT_PBKDF2_HASH:
			{
				crypt::Hash cur_sel = crypt::help::getHashByIndex(::SendDlgItemMessage(tab.key, IDC_CRYPT_PBKDF2_HASH, CB_GETCURSEL, 0, 0), crypt::HMAC_SUPPORT);
				updateHashDigestControl(cur_sel, tab.key, IDC_CRYPT_PBKDF2_HASH_LENGTH);
				::SendDlgItemMessage(tab.key, IDC_CRYPT_PBKDF2_HASH_LENGTH, CB_SETCURSEL, 0, 0);
				break;
			}
			case IDC_CRYPT_IV_ENC:
			{
				crypt::UserData ivdata;
				invalid.iv = !checkCustomIV(ivdata, true);
				InvalidateRect(::GetDlgItem(tab.iv, IDC_CRYPT_IV_INPUT), NULL, NULL);
				::SetFocus(::GetDlgItem(tab.iv, IDC_CRYPT_IV_INPUT));
				break;
			}
			case IDC_CRYPT_AUTH_KEY_LIST:
			{
				::SendDlgItemMessage(tab.auth, IDC_CRYPT_AUTH_KEY_PRESET, BM_SETCHECK, true, 0);
				::SendDlgItemMessage(tab.auth, IDC_CRYPT_AUTH_KEY_CUSTOM, BM_SETCHECK, false, 0);
				break;
			}
			case IDC_CRYPT_AUTH_HASH:
			{
				crypt::Hash cur_sel = crypt::help::getHashByIndex(::SendDlgItemMessage(tab.auth, IDC_CRYPT_AUTH_HASH, CB_GETCURSEL, 0, 0), crypt::HMAC_SUPPORT);
				updateHashDigestControl(cur_sel, tab.auth, IDC_CRYPT_AUTH_HASH_LENGTH);
				::SendDlgItemMessage(tab.auth, IDC_CRYPT_AUTH_HASH_LENGTH, CB_SETCURSEL, 0, 0);
				::SetFocus(::GetDlgItem(tab.auth, IDC_CRYPT_AUTH_PW_VALUE));
				break;
			}
			case IDC_CRYPT_AUTH_PW_ENC:
			{
				invalid.hmac_key = !checkHMACKey(crypt->hmac.hash.key, true);
				InvalidateRect(::GetDlgItem(tab.auth, IDC_CRYPT_AUTH_PW_VALUE), NULL, NULL);
				::SetFocus(::GetDlgItem(tab.auth, IDC_CRYPT_AUTH_PW_VALUE));
				break;
			}
			}
			break;
		}
		case EN_SETFOCUS:
		{
			switch (LOWORD(wParam))
			{
			case IDC_CRYPT_AUTH_PW_VALUE:
			{
				::SendDlgItemMessage(tab.auth, IDC_CRYPT_AUTH_KEY_CUSTOM, BM_SETCHECK, true, 0);
				::SendDlgItemMessage(tab.auth, IDC_CRYPT_AUTH_KEY_PRESET, BM_SETCHECK, false, 0);
				break;
			}
			}
			break;
		}
		case EN_CHANGE:
		{
			checkSpinControlValue(LOWORD(wParam));
			if (LOWORD(wParam) == IDC_CRYPT_IV_INPUT) {
				crypt::UserData ivdata;
				invalid.iv = !checkCustomIV(ivdata, false);
				InvalidateRect(::GetDlgItem(tab.iv, IDC_CRYPT_IV_INPUT), NULL, NULL);
			} else if (LOWORD(wParam) == IDC_CRYPT_AUTH_PW_VALUE) {
				invalid.hmac_key = !checkHMACKey(crypt->hmac.hash.key, false);
				InvalidateRect(::GetDlgItem(tab.auth, IDC_CRYPT_AUTH_PW_VALUE), NULL, NULL);
			} else if (LOWORD(wParam) == IDC_CRYPT_PASSWORD) {
				crypt::secure_string temp;
				checkPassword(temp, false);
			}
			break;
		}
		}
		break;
	}
	case WM_NOTIFY:
	{
		if (((LPNMHDR)lParam)->code == TCN_SELCHANGE) {
			changeActiveTab(TabCtrl_GetCurSel(((LPNMHDR)lParam)->hwndFrom));
		}
		break;
	}
	case WM_CTLCOLOREDIT:
	{
		if ((invalid.iv && (HWND)lParam == GetDlgItem(tab.iv, IDC_CRYPT_IV_INPUT)) || 
			(invalid.hmac_key && (HWND)lParam == GetDlgItem(tab.auth, IDC_CRYPT_AUTH_PW_VALUE)) ||
			(invalid.password && (HWND)lParam == GetDlgItem(tab.basic, IDC_CRYPT_PASSWORD))) {
			SetBkMode((HDC)wParam, TRANSPARENT);
			return (INT_PTR)invalid.brush;
		}
		break;
	}
	}
	return FALSE;
}

void DlgCrypt::setupDialog()
{
	std::wstring temp_str;

	// ------- Caption
	std::wstring caption = (operation == Operation::Enc) ? TEXT("nppcrypt::encryption ") : TEXT("nppcrypt::decryption ");
	if (filename && filename->size() > 0) {
		if (filename->size() > 20) {
			caption += (TEXT("(") + filename->substr(0,20) + TEXT("...)"));
		} else {
			caption += (TEXT("(") + *filename + TEXT(")"));
		}
	}
	SetWindowText(_hSelf, caption.c_str());

	// ------- Tab-Control
	HWND hTab = ::GetDlgItem(_hSelf, IDC_CRYPT_TAB);
	TCITEM tie = { 0 };
	tie.mask = TCIF_TEXT;
	tie.pszText = TEXT("basic");
	TabCtrl_InsertItem(hTab, 0, &tie);
	tie.pszText = TEXT("encoding");
	TabCtrl_InsertItem(hTab, 1, &tie);
	tie.pszText = TEXT("key");
	TabCtrl_InsertItem(hTab, 2, &tie);
	tie.pszText = TEXT("iv");
	TabCtrl_InsertItem(hTab, 3, &tie);
	tie.pszText = TEXT("auth");
	TabCtrl_InsertItem(hTab, 4, &tie);

	HINSTANCE hinst = helper::NPP::getDLLHandle();
	tab.basic = CreateDialogParam(hinst, MAKEINTRESOURCE(IDD_CRYPT_BASIC), hTab, (DLGPROC)dlgProc, (LPARAM)this);
	tab.key = CreateDialogParam(hinst, MAKEINTRESOURCE(IDD_CRYPT_KEY), hTab, (DLGPROC)dlgProc, (LPARAM)this);
	tab.auth = CreateDialogParam(hinst, MAKEINTRESOURCE(IDD_CRYPT_AUTH), hTab, (DLGPROC)dlgProc, (LPARAM)this);
	tab.iv = CreateDialogParam(hinst, MAKEINTRESOURCE(IDD_CRYPT_IV), hTab, (DLGPROC)dlgProc, (LPARAM)this);
	tab.encoding = CreateDialogParam(hinst, MAKEINTRESOURCE(IDD_CRYPT_ENCODING), hTab, (DLGPROC)dlgProc, (LPARAM)this);

	RECT rc;
	GetClientRect(hTab, &rc);
	TabCtrl_AdjustRect(hTab, FALSE, &rc);
	MoveWindow(tab.basic, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);
	MoveWindow(tab.auth, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, FALSE);
	MoveWindow(tab.key, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, FALSE);
	MoveWindow(tab.iv, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, FALSE);
	MoveWindow(tab.encoding, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, FALSE);

	// ------- Cipher -----------------------------------------------------------
	current.cipher = crypt->options.cipher;
	int category = crypt::help::getCipherCategory(current.cipher);

	for (crypt::help::CipherCategories it; *it; ++it) {
		::SendDlgItemMessage(tab.basic, IDC_CRYPT_CIPHER_TYPE, CB_ADDSTRING, 0, (LPARAM)helper::Windows::ToWCHAR(*it).c_str());
	}
	::SendDlgItemMessage(tab.basic, IDC_CRYPT_CIPHER_TYPE, CB_SETCURSEL, category, 0);
	::SendDlgItemMessage(tab.basic, IDC_CRYPT_CIPHER, CB_RESETCONTENT, 0, 0);
	for (crypt::help::CipherNames it(category); *it; ++it) {
		::SendDlgItemMessage(tab.basic, IDC_CRYPT_CIPHER, CB_ADDSTRING, 0, (LPARAM)helper::Windows::ToWCHAR(*it).c_str());
	}
	::SendDlgItemMessage(tab.basic, IDC_CRYPT_CIPHER, CB_SETCURSEL, crypt::help::getCipherIndex(current.cipher), 0);

	// ------- Cipher Modes ------------------------------------------------------
	for(crypt::help::CipherModes it(current.cipher); *it; ++it) {
		::SendDlgItemMessage(tab.basic, IDC_CRYPT_MODE, CB_ADDSTRING, 0, (LPARAM)helper::Windows::ToWCHAR(*it).c_str());
	}
	::SendDlgItemMessage(tab.basic, IDC_CRYPT_MODE, CB_SETCURSEL, crypt::help::getModeIndex(current.cipher, crypt->options.mode), 0);

	help.cipher.setup(_hInst, tab.basic, ::GetDlgItem(tab.basic, IDC_CRYPT_HELP_CIPHER));
	help.mode.setup(_hInst, tab.basic, ::GetDlgItem(tab.basic, IDC_CRYPT_HELP_MODE));
	updateCipherModeInfo(crypt->options.mode);

	current.key_length = crypt->options.key.length;
	updateCipherControls();

	// ------- Password
	::SendDlgItemMessage(tab.basic, IDC_CRYPT_PASSWORD, EM_LIMITTEXT, NPPC_PASSWORD_MAXLENGTH, 0);
	::SendDlgItemMessage(tab.basic, IDC_CRYPT_PASSWORD, EM_SETPASSWORDCHAR, '*', 0);
	setupInputEncodingSelect(tab.basic, IDC_CRYPT_PASSWORD_ENC);

	// ------- Encoding
	// no binary output if buffer is 16 bit
	if (operation == Operation::Enc && !helper::Buffer::isCurrent8Bit()) {
		if (crypt->options.encoding.enc == crypt::Encoding::ascii) {
			crypt->options.encoding.enc = crypt::Encoding::base16;
		}
		::EnableWindow(::GetDlgItem(tab.encoding, IDC_CRYPT_ENC_ASCII), false);
	}
	::SendDlgItemMessage(tab.encoding, IDC_CRYPT_ENC_ASCII, BM_SETCHECK, (crypt->options.encoding.enc == crypt::Encoding::ascii), 0);
	::SendDlgItemMessage(tab.encoding, IDC_CRYPT_ENC_BASE16, BM_SETCHECK, (crypt->options.encoding.enc == crypt::Encoding::base16), 0);
	::SendDlgItemMessage(tab.encoding, IDC_CRYPT_ENC_BASE32, BM_SETCHECK, (crypt->options.encoding.enc == crypt::Encoding::base32), 0);
	::SendDlgItemMessage(tab.encoding, IDC_CRYPT_ENC_BASE64, BM_SETCHECK, (crypt->options.encoding.enc == crypt::Encoding::base64), 0);

	::SendDlgItemMessage(tab.encoding, IDC_CRYPT_ENC_LINEBREAK, BM_SETCHECK, crypt->options.encoding.linebreaks, 0);
	::SendDlgItemMessage(tab.encoding, IDC_CRYPT_ENC_LB_WIN, BM_SETCHECK, (crypt->options.encoding.eol == crypt::EOL::windows), 0);
	::SendDlgItemMessage(tab.encoding, IDC_CRYPT_ENC_LB_UNIX, BM_SETCHECK, (crypt->options.encoding.eol != crypt::EOL::windows), 0);
	::SendDlgItemMessage(tab.encoding, IDC_CRYPT_ENC_UPPERCASE, BM_SETCHECK, crypt->options.encoding.uppercase, 0);

	::SendDlgItemMessage(tab.encoding, IDC_CRYPT_ENC_LINELEN_SPIN, UDM_SETRANGE32, 1, NPPC_MAX_LINE_LENGTH);
	::SendDlgItemMessage(tab.encoding, IDC_CRYPT_ENC_LINELEN_SPIN, UDM_SETBUDDY, (WPARAM)GetDlgItem(tab.encoding, IDC_CRYPT_ENC_LINELEN), 0);
	::SendDlgItemMessage(tab.encoding, IDC_CRYPT_ENC_LINELEN_SPIN, UDM_SETPOS32, 0, crypt->options.encoding.linelength);

	help.encoding.setup(_hInst, tab.encoding, ::GetDlgItem(tab.encoding, IDC_CRYPT_ENC_HELP));
	updateEncodingControls(crypt->options.encoding.enc);

	if (operation == Operation::Dec) {
		::EnableWindow(::GetDlgItem(tab.encoding, IDC_CRYPT_ENC_LINEBREAK), false);
		::EnableWindow(::GetDlgItem(tab.encoding, IDC_CRYPT_ENC_LB_WIN), false);
		::EnableWindow(::GetDlgItem(tab.encoding, IDC_CRYPT_ENC_LB_UNIX), false);
		::EnableWindow(::GetDlgItem(tab.encoding, IDC_CRYPT_ENC_UPPERCASE), false);
		::EnableWindow(::GetDlgItem(tab.encoding, IDC_CRYPT_ENC_LINELEN_SPIN), false);
		::EnableWindow(::GetDlgItem(tab.encoding, IDC_CRYPT_ENC_LINELEN), false);
		::EnableWindow(::GetDlgItem(tab.encoding, IDC_CRYPT_ENC_STATIC), false);
	}

	// ------- Key-Derivation
	::SendDlgItemMessage(tab.key, IDC_CRYPT_SALT_SPIN, UDM_SETRANGE32, 1, crypt::Constants::salt_max);
	::SendDlgItemMessage(tab.key, IDC_CRYPT_SALT_SPIN, UDM_SETBUDDY, (WPARAM)GetDlgItem(tab.key, IDC_CRYPT_SALT_BYTES), 0);
	::SendDlgItemMessage(tab.key, IDC_CRYPT_SALT_SPIN, UDM_SETPOS32, 0, crypt->options.key.salt_bytes);
	::SendDlgItemMessage(tab.key, IDC_CRYPT_SALT, BM_SETCHECK, (crypt->options.key.salt_bytes > 0), 0);

	for (crypt::help::Hashnames it(crypt::HMAC_SUPPORT); *it; ++it) {
		::SendDlgItemMessage(tab.key, IDC_CRYPT_PBKDF2_HASH, CB_ADDSTRING, 0, (LPARAM)helper::Windows::ToWCHAR(*it).c_str());
	}
	::SendDlgItemMessage(tab.key, IDC_CRYPT_PBKDF2_ITER_SPIN, UDM_SETRANGE32, crypt::Constants::pbkdf2_iter_min, crypt::Constants::pbkdf2_iter_max);
	::SendDlgItemMessage(tab.key, IDC_CRYPT_PBKDF2_ITER_SPIN, UDM_SETBUDDY, (WPARAM)GetDlgItem(tab.key, IDC_CRYPT_PBKDF2_ITER), 0);
	::SendDlgItemMessage(tab.key, IDC_CRYPT_BCRYPT_ITER_SPIN, UDM_SETRANGE32, crypt::Constants::bcrypt_iter_min, crypt::Constants::bcrypt_iter_max);
	::SendDlgItemMessage(tab.key, IDC_CRYPT_BCRYPT_ITER_SPIN, UDM_SETBUDDY, (WPARAM)GetDlgItem(tab.key, IDC_CRYPT_BCRYPT_ITER), 0);
	::SendDlgItemMessage(tab.key, IDC_CRYPT_SCRYPT_N_SPIN, UDM_SETRANGE32, crypt::Constants::scrypt_N_min, crypt::Constants::scrypt_N_max);
	::SendDlgItemMessage(tab.key, IDC_CRYPT_SCRYPT_N_SPIN, UDM_SETBUDDY, (WPARAM)GetDlgItem(tab.key, IDC_CRYPT_SCRYPT_N), 0);
	::SendDlgItemMessage(tab.key, IDC_CRYPT_SCRYPT_R_SPIN, UDM_SETRANGE32, crypt::Constants::scrypt_r_min, crypt::Constants::scrypt_r_max);
	::SendDlgItemMessage(tab.key, IDC_CRYPT_SCRYPT_R_SPIN, UDM_SETBUDDY, (WPARAM)GetDlgItem(tab.key, IDC_CRYPT_SCRYPT_R), 0);
	::SendDlgItemMessage(tab.key, IDC_CRYPT_SCRYPT_P_SPIN, UDM_SETRANGE32, crypt::Constants::scrypt_p_min, crypt::Constants::scrypt_p_max);
	::SendDlgItemMessage(tab.key, IDC_CRYPT_SCRYPT_P_SPIN, UDM_SETBUDDY, (WPARAM)GetDlgItem(tab.key, IDC_CRYPT_SCRYPT_P), 0);
	::SendDlgItemMessage(tab.key, IDC_CRYPT_PBKDF2_HASH, CB_SETCURSEL, crypt::help::getHashIndex(crypt::Constants::pbkdf2_default_hash, crypt::HMAC_SUPPORT), 0);
	::SendDlgItemMessage(tab.key, IDC_CRYPT_PBKDF2_ITER_SPIN, UDM_SETPOS32, 0, crypt::Constants::pbkdf2_iter_default);
	::SendDlgItemMessage(tab.key, IDC_CRYPT_BCRYPT_ITER_SPIN, UDM_SETPOS32, 0, crypt::Constants::bcrypt_iter_default);
	::SendDlgItemMessage(tab.key, IDC_CRYPT_SCRYPT_N_SPIN, UDM_SETPOS32, 0, crypt::Constants::scrypt_N_default);
	::SendDlgItemMessage(tab.key, IDC_CRYPT_SCRYPT_R_SPIN, UDM_SETPOS32, 0, crypt::Constants::scrypt_r_default);
	::SendDlgItemMessage(tab.key, IDC_CRYPT_SCRYPT_P_SPIN, UDM_SETPOS32, 0, crypt::Constants::scrypt_p_default);

	current.key_derivation = crypt->options.key.algorithm;
	switch (crypt->options.key.algorithm) {
	case crypt::KeyDerivation::pbkdf2:
		::SendDlgItemMessage(tab.key, IDC_CRYPT_KEY_PBKDF2, BM_SETCHECK, true, 0);
		::SendDlgItemMessage(tab.key, IDC_CRYPT_PBKDF2_HASH, CB_SETCURSEL, crypt::help::getHashIndex((crypt::Hash)crypt->options.key.options[0],crypt::HMAC_SUPPORT), 0);
		::SendDlgItemMessage(tab.key, IDC_CRYPT_PBKDF2_ITER_SPIN, UDM_SETPOS32, 0, crypt->options.key.options[2]);
		break;
	case crypt::KeyDerivation::bcrypt:
		::SendDlgItemMessage(tab.key, IDC_CRYPT_KEY_BCRYPT, BM_SETCHECK, true, 0);
		::SendDlgItemMessage(tab.key, IDC_CRYPT_BCRYPT_ITER_SPIN, UDM_SETPOS32, 0, crypt->options.key.options[0]);
		break;
	case crypt::KeyDerivation::scrypt:
		::SendDlgItemMessage(tab.key, IDC_CRYPT_KEY_SCRYPT, BM_SETCHECK, true, 0);
		::SendDlgItemMessage(tab.key, IDC_CRYPT_SCRYPT_N_SPIN, UDM_SETPOS32, 0, crypt->options.key.options[0]);
		::SendDlgItemMessage(tab.key, IDC_CRYPT_SCRYPT_R_SPIN, UDM_SETPOS32, 0, crypt->options.key.options[1]);
		::SendDlgItemMessage(tab.key, IDC_CRYPT_SCRYPT_P_SPIN, UDM_SETPOS32, 0, crypt->options.key.options[2]);
		break;
	}
	crypt::Hash cur_sel = crypt::help::getHashByIndex(::SendDlgItemMessage(tab.key, IDC_CRYPT_PBKDF2_HASH, CB_GETCURSEL, 0, 0), crypt::HMAC_SUPPORT);
	updateHashDigestControl(cur_sel, tab.key, IDC_CRYPT_PBKDF2_HASH_LENGTH);
	if (crypt->options.key.algorithm == crypt::KeyDerivation::pbkdf2) {
		::SendDlgItemMessage(tab.key, IDC_CRYPT_PBKDF2_HASH_LENGTH, CB_SETCURSEL, crypt::help::getHashDigestIndex(cur_sel, crypt->options.key.options[1]), 0);
	} else {
		::SendDlgItemMessage(tab.key, IDC_CRYPT_PBKDF2_HASH_LENGTH, CB_SETCURSEL, crypt::help::getHashDigestIndex(cur_sel, crypt::Constants::pbkdf2_default_hash_digest), 0);
	}

	help.salt.setup(_hInst, tab.key, ::GetDlgItem(tab.key, IDC_CRYPT_HELP_SALT));
	help.keyalgorithm.setup(_hInst, tab.key, ::GetDlgItem(tab.key, IDC_CRYPT_HELP_KEYALGO));
	help.salt.setURL(NPPC_CRYPT_SALT_HELP_URL);
	help.salt.setTooltip(NPPC_CRYPT_SALT_HELP);

	updateKeyDerivationControls();

	// ------- IV
	::SendDlgItemMessage(tab.iv, IDC_CRYPT_IV_RANDOM, BM_SETCHECK, (crypt->options.iv == crypt::IV::random), 0);
	::SendDlgItemMessage(tab.iv, IDC_CRYPT_IV_KEY, BM_SETCHECK, (crypt->options.iv == crypt::IV::keyderivation), 0);
	::SendDlgItemMessage(tab.iv, IDC_CRYPT_IV_ZERO, BM_SETCHECK, (crypt->options.iv == crypt::IV::zero), 0);
	::SendDlgItemMessage(tab.iv, IDC_CRYPT_IV_CUSTOM, BM_SETCHECK, (crypt->options.iv == crypt::IV::custom), 0);
	setupInputEncodingSelect(tab.iv, IDC_CRYPT_IV_ENC);
	::SendDlgItemMessage(tab.iv, IDC_CRYPT_IV_INPUT, EM_LIMITTEXT, NPPC_MAX_CUSTOM_IV_LENGTH, 0);

	help.iv.setup(_hInst, tab.iv, ::GetDlgItem(tab.iv, IDC_CRYPT_HELP_IV));
	help.iv.setURL(NPPC_CRYPT_IV_HELP_URL);
	help.iv.setTooltip(crypt::help::getInfo(crypt->options.iv));
	if(operation == Operation::Dec) {
		if ((crypt->options.iv == crypt::IV::random || crypt->options.iv == crypt::IV::custom) && ivdata->size()) {
			crypt::secure_string temp;
			ivdata->get(temp, crypt::Encoding::base64);
			::SendDlgItemMessage(tab.iv, IDC_CRYPT_IV_ENC, CB_SETCURSEL, (int)crypt::Encoding::base64, 0);
			setText(IDC_CRYPT_IV_INPUT, temp, tab.iv);
		}
		if (crypt->options.iv == crypt::IV::random) {
			::SendDlgItemMessage(tab.iv, IDC_CRYPT_IV_CUSTOM, BM_SETCHECK, true, 0);
			help.iv.setTooltip(crypt::help::getInfo(crypt::IV::custom));
		}
		::EnableWindow(::GetDlgItem(tab.iv, IDC_CRYPT_IV_RANDOM), false);
	}
	if (crypt->options.mode == crypt::Mode::ecb) {
		updateIVControls(true);
	}


	if (!::SendDlgItemMessage(tab.iv, IDC_CRYPT_IV_CUSTOM, BM_GETCHECK, 0, 0)) {
		::EnableWindow(::GetDlgItem(tab.iv, IDC_CRYPT_IV_INPUT), false);
		::EnableWindow(::GetDlgItem(tab.iv, IDC_CRYPT_IV_ENC), false);
	}	

	// ------- Auth
	::SendDlgItemMessage(tab.auth, IDC_CRYPT_AUTH_ENABLE, BM_SETCHECK, crypt->hmac.enable, 0);
	for(crypt::help::Hashnames it(crypt::HMAC_SUPPORT); *it; ++it) {
		::SendDlgItemMessage(tab.auth, IDC_CRYPT_AUTH_HASH, CB_ADDSTRING, 0, (LPARAM)helper::Windows::ToWCHAR(*it).c_str());
	}
	::SendDlgItemMessage(tab.auth, IDC_CRYPT_AUTH_HASH, CB_SETCURSEL, crypt::help::getHashIndex(crypt->hmac.hash.algorithm, crypt::HMAC_SUPPORT), 0);
	updateHashDigestControl(crypt->hmac.hash.algorithm, tab.auth, IDC_CRYPT_AUTH_HASH_LENGTH);
	::SendDlgItemMessage(tab.auth, IDC_CRYPT_AUTH_HASH_LENGTH, CB_SETCURSEL, crypt::help::getHashDigestIndex(crypt->hmac.hash.algorithm, crypt->hmac.hash.digest_length), 0);
	for (size_t i = 0; i < preferences.getKeyNum(); i++) {
		::SendDlgItemMessage(tab.auth, IDC_CRYPT_AUTH_KEY_LIST, CB_ADDSTRING, 0, (LPARAM)preferences.getKeyLabel(i));
	}
	if (crypt->hmac.keypreset_id >= (int)preferences.getKeyNum() || crypt->hmac.keypreset_id < -1) {
		crypt->hmac.keypreset_id = 0;
	}
	if (crypt->hmac.keypreset_id >= 0) {
		::SendDlgItemMessage(tab.auth, IDC_CRYPT_AUTH_KEY_LIST, CB_SETCURSEL, crypt->hmac.keypreset_id, 0);
	} else {
		::SendDlgItemMessage(tab.auth, IDC_CRYPT_AUTH_KEY_LIST, CB_SETCURSEL, 0, 0);
	}
	::SendDlgItemMessage(tab.auth, IDC_CRYPT_AUTH_KEY_PRESET, BM_SETCHECK, (crypt->hmac.keypreset_id >= 0), 0);
	::SendDlgItemMessage(tab.auth, IDC_CRYPT_AUTH_KEY_CUSTOM, BM_SETCHECK, (crypt->hmac.keypreset_id < 0), 0);
	::SendDlgItemMessage(tab.auth, IDC_CRYPT_AUTH_PW_VALUE, EM_SETPASSWORDCHAR, '*', 0);
	::SendDlgItemMessage(tab.auth, IDC_CRYPT_AUTH_PW_VALUE, EM_LIMITTEXT, NPPC_MAX_HMAC_KEYLENGTH, 0);
	setupInputEncodingSelect(tab.auth, IDC_CRYPT_AUTH_PW_ENC);

	help.auth.setup(_hInst, tab.auth, ::GetDlgItem(tab.auth, IDC_CRYPT_AUTH_HELP));

	if (operation == Operation::Dec) {
		help.auth.enable(false);
		::EnableWindow(::GetDlgItem(tab.auth, IDC_CRYPT_AUTH_ENABLE), false);
		::EnableWindow(::GetDlgItem(tab.auth, IDC_CRYPT_AUTH_HASH), false);
		::EnableWindow(::GetDlgItem(tab.auth, IDC_CRYPT_AUTH_HASH_LENGTH), false);
		::EnableWindow(::GetDlgItem(tab.auth, IDC_CRYPT_AUTH_KEY_PRESET), false);
		::EnableWindow(::GetDlgItem(tab.auth, IDC_CRYPT_AUTH_KEY_CUSTOM), false);
		::EnableWindow(::GetDlgItem(tab.auth, IDC_CRYPT_AUTH_KEY_LIST), false);
		::EnableWindow(::GetDlgItem(tab.auth, IDC_CRYPT_AUTH_PW_VALUE), false);
		::EnableWindow(::GetDlgItem(tab.auth, IDC_CRYPT_AUTH_PW_SHOW), false);
		::EnableWindow(::GetDlgItem(tab.auth, IDC_CRYPT_AUTH_PW_ENC), false);
		::EnableWindow(::GetDlgItem(tab.auth, IDC_CRYPT_AUTH_STATIC1), false);
	} else {
		help.auth.setURL(NPPC_CRYPT_HMAC_HELP_URL);
		help.auth.setTooltip("add additional HMAC to authenticate header and encrypted data");
		::EnableWindow(::GetDlgItem(tab.auth, IDC_CRYPT_AUTH_HASH), crypt->hmac.enable);
		::EnableWindow(::GetDlgItem(tab.auth, IDC_CRYPT_AUTH_HASH_LENGTH), crypt->hmac.enable);
		::EnableWindow(::GetDlgItem(tab.auth, IDC_CRYPT_AUTH_KEY_PRESET), crypt->hmac.enable);
		::EnableWindow(::GetDlgItem(tab.auth, IDC_CRYPT_AUTH_KEY_CUSTOM), crypt->hmac.enable);
		::EnableWindow(::GetDlgItem(tab.auth, IDC_CRYPT_AUTH_KEY_LIST), crypt->hmac.enable);
		::EnableWindow(::GetDlgItem(tab.auth, IDC_CRYPT_AUTH_PW_VALUE), crypt->hmac.enable);
		::EnableWindow(::GetDlgItem(tab.auth, IDC_CRYPT_AUTH_PW_SHOW), crypt->hmac.enable);
		::EnableWindow(::GetDlgItem(tab.auth, IDC_CRYPT_AUTH_PW_ENC), crypt->hmac.enable);
		::EnableWindow(::GetDlgItem(tab.auth, IDC_CRYPT_AUTH_STATIC1), crypt->hmac.enable);
	}
	// ------- 
	changeActiveTab(0);
}

void DlgCrypt::changeActiveTab(int id)
{
	if (current.tab == 3 && id != 3) {
		if (::IsWindowEnabled(::GetDlgItem(tab.iv, IDC_CRYPT_IV_CUSTOM)) && ::SendDlgItemMessage(tab.iv, IDC_CRYPT_IV_CUSTOM, BM_GETCHECK, 0, 0)) {
			crypt::UserData ivdata;
			invalid.iv = !checkCustomIV(ivdata, true);
			if (invalid.iv) {
				TabCtrl_SetCurSel(::GetDlgItem(_hSelf, IDC_CRYPT_TAB), 3);
				InvalidateRect(::GetDlgItem(tab.iv, IDC_CRYPT_IV_INPUT), NULL, NULL);
				return;
			}
		}
	} else if (operation == Operation::Enc && current.tab == 4 && id != 4) {
		if (!!::SendDlgItemMessage(tab.auth, IDC_CRYPT_AUTH_ENABLE, BM_GETCHECK, 0, 0) &&
			!!::SendDlgItemMessage(tab.auth, IDC_CRYPT_AUTH_KEY_CUSTOM, BM_GETCHECK, 0, 0)) {
			invalid.hmac_key = !checkHMACKey(crypt->hmac.hash.key, true);
			if (invalid.hmac_key) {
				TabCtrl_SetCurSel(::GetDlgItem(_hSelf, IDC_CRYPT_TAB), 4);
				InvalidateRect(::GetDlgItem(tab.auth, IDC_CRYPT_AUTH_PW_VALUE), NULL, NULL);
				return;
			}
		}
	}

	switch (id)
	{
	case 0:
	{
		ShowWindow(tab.basic, SW_SHOW);
		ShowWindow(tab.encoding, SW_HIDE);
		ShowWindow(tab.key, SW_HIDE);
		ShowWindow(tab.iv, SW_HIDE);
		ShowWindow(tab.auth, SW_HIDE);
		::EnableWindow(::GetDlgItem(_hSelf, IDC_OK), true);
		PostMessage(tab.basic, WM_NEXTDLGCTL, (WPARAM)::GetDlgItem(tab.basic, IDC_CRYPT_PASSWORD), TRUE);
		break;
	}
	case 1:
	{
		ShowWindow(tab.basic, SW_HIDE);
		ShowWindow(tab.encoding, SW_SHOW);
		ShowWindow(tab.key, SW_HIDE);
		ShowWindow(tab.iv, SW_HIDE);
		ShowWindow(tab.auth, SW_HIDE);
		::EnableWindow(::GetDlgItem(_hSelf, IDC_OK), false);
		break;
	}
	case 2:
	{
		ShowWindow(tab.basic, SW_HIDE);
		ShowWindow(tab.encoding, SW_HIDE);
		ShowWindow(tab.key, SW_SHOW);
		ShowWindow(tab.iv, SW_HIDE);
		ShowWindow(tab.auth, SW_HIDE);
		::EnableWindow(::GetDlgItem(_hSelf, IDC_OK), false);
		break;
	}
	case 3:
	{
		ShowWindow(tab.basic, SW_HIDE);
		ShowWindow(tab.encoding, SW_HIDE);
		ShowWindow(tab.key, SW_HIDE);
		ShowWindow(tab.iv, SW_SHOW);
		ShowWindow(tab.auth, SW_HIDE);
		::EnableWindow(::GetDlgItem(_hSelf, IDC_OK), false);
		if (::SendDlgItemMessage(tab.iv, IDC_CRYPT_IV_CUSTOM, BM_GETCHECK, 0, 0)) {
			::SetFocus(::GetDlgItem(tab.iv, IDC_CRYPT_IV_INPUT));
		}
		break;
	}
	case 4:
	{
		ShowWindow(tab.basic, SW_HIDE);
		ShowWindow(tab.encoding, SW_HIDE);
		ShowWindow(tab.key, SW_HIDE);
		ShowWindow(tab.iv, SW_HIDE);
		ShowWindow(tab.auth, SW_SHOW);
		::EnableWindow(::GetDlgItem(_hSelf, IDC_OK), false);
		if (::SendDlgItemMessage(tab.auth, IDC_CRYPT_AUTH_KEY_CUSTOM, BM_GETCHECK, 0, 0)) {
			::SetFocus(::GetDlgItem(tab.auth, IDC_CRYPT_AUTH_PW_VALUE));
		}
		break;
	}
	}
	current.tab = id;
}


bool DlgCrypt::prepareOptions()
{
	try
	{
		// ------- cipher, keylength, mode
		int cipher_index = (int)::SendDlgItemMessage(tab.basic, IDC_CRYPT_CIPHER, CB_GETCURSEL, 0, 0);
		int cipher_cat = (int)::SendDlgItemMessage(tab.basic, IDC_CRYPT_CIPHER_TYPE, CB_GETCURSEL, 0, 0);
		int cipher_keylength = (int)::SendDlgItemMessage(tab.basic, IDC_CRYPT_KEYLENGTH, CB_GETCURSEL, 0, 0);
		crypt->options.cipher = crypt::help::getCipherByIndex(cipher_cat, cipher_index);
		crypt->options.key.length = crypt::help::getCipherKeylengthByIndex(crypt->options.cipher, cipher_keylength);
		int t_mode = (int)::SendDlgItemMessage(tab.basic, IDC_CRYPT_MODE, CB_GETCURSEL, 0, 0);
		crypt->options.mode = (t_mode >= 0) ? crypt::help::getModeByIndex(crypt->options.cipher, t_mode) : crypt::Mode::cbc;

		// ------- encoding
		if (::SendDlgItemMessage(tab.encoding, IDC_CRYPT_ENC_ASCII, BM_GETCHECK, 0, 0)) {
			crypt->options.encoding.enc = crypt::Encoding::ascii;
		} else if (::SendDlgItemMessage(tab.encoding, IDC_CRYPT_ENC_BASE16, BM_GETCHECK, 0, 0)) {
			crypt->options.encoding.enc = crypt::Encoding::base16;
		} else if (::SendDlgItemMessage(tab.encoding, IDC_CRYPT_ENC_BASE32, BM_GETCHECK, 0, 0)) {
			crypt->options.encoding.enc = crypt::Encoding::base32;
		} else {
			crypt->options.encoding.enc = crypt::Encoding::base64;
		}
		crypt->options.encoding.linebreaks = !!::SendDlgItemMessage(tab.encoding, IDC_CRYPT_ENC_LINEBREAK, BM_GETCHECK, 0, 0);
		crypt->options.encoding.uppercase = !!::SendDlgItemMessage(tab.encoding, IDC_CRYPT_ENC_UPPERCASE, BM_GETCHECK, 0, 0);
		crypt->options.encoding.eol = !!::SendDlgItemMessage(tab.encoding, IDC_CRYPT_ENC_LB_WIN, BM_GETCHECK, 0, 0) ? crypt::EOL::windows : crypt::EOL::unix;
		crypt->options.encoding.linelength = (int)::SendDlgItemMessage(tab.encoding, IDC_CRYPT_ENC_LINELEN_SPIN, UDM_GETPOS32, 0, 0);

		// ------- salt
		if (::SendDlgItemMessage(tab.key, IDC_CRYPT_SALT, BM_GETCHECK, 0, 0)) {
			crypt->options.key.salt_bytes = (int)::SendDlgItemMessage(tab.key, IDC_CRYPT_SALT_SPIN, UDM_GETPOS32, 0, 0);
		} else {
			crypt->options.key.salt_bytes = 0;
		}

		// ------- key derivation
		if (::SendDlgItemMessage(tab.key, IDC_CRYPT_KEY_PBKDF2, BM_GETCHECK, 0, 0)) {
			crypt->options.key.algorithm = crypt::KeyDerivation::pbkdf2;
			crypt::Hash h = crypt::help::getHashByIndex(::SendDlgItemMessage(tab.key, IDC_CRYPT_PBKDF2_HASH, CB_GETCURSEL, 0, 0), crypt::HMAC_SUPPORT);
			crypt->options.key.options[0] = (int)h;
			crypt->options.key.options[1] = crypt::help::getHashDigestByIndex(h, ::SendDlgItemMessage(tab.key, IDC_CRYPT_PBKDF2_HASH_LENGTH, CB_GETCURSEL, 0, 0));
			crypt->options.key.options[2] = (int)::SendDlgItemMessage(tab.key, IDC_CRYPT_PBKDF2_ITER_SPIN, UDM_GETPOS32, 0, 0);
		} else if (::SendDlgItemMessage(tab.key, IDC_CRYPT_KEY_BCRYPT, BM_GETCHECK, 0, 0)) {
			crypt->options.key.algorithm = crypt::KeyDerivation::bcrypt;
			crypt->options.key.options[0] = (int)::SendDlgItemMessage(tab.key, IDC_CRYPT_BCRYPT_ITER_SPIN, UDM_GETPOS32, 0, 0);
			crypt->options.key.options[1] = 0;
			crypt->options.key.options[2] = 0;
		} else {
			crypt->options.key.algorithm = crypt::KeyDerivation::scrypt;
			crypt->options.key.options[0] = (int)::SendDlgItemMessage(tab.key, IDC_CRYPT_SCRYPT_N_SPIN, UDM_GETPOS32, 0, 0);
			crypt->options.key.options[1] = (int)::SendDlgItemMessage(tab.key, IDC_CRYPT_SCRYPT_R_SPIN, UDM_GETPOS32, 0, 0);
			crypt->options.key.options[2] = (int)::SendDlgItemMessage(tab.key, IDC_CRYPT_SCRYPT_P_SPIN, UDM_GETPOS32, 0, 0);
		}

		// ------- iv
		if (crypt->options.mode != crypt::Mode::ecb) {
			if (::SendDlgItemMessage(tab.iv, IDC_CRYPT_IV_RANDOM, BM_GETCHECK, 0, 0)) {
				crypt->options.iv = crypt::IV::random;
			} else if (::SendDlgItemMessage(tab.iv, IDC_CRYPT_IV_KEY, BM_GETCHECK, 0, 0)) {
				crypt->options.iv = crypt::IV::keyderivation;
			} else if (::SendDlgItemMessage(tab.iv, IDC_CRYPT_IV_ZERO, BM_GETCHECK, 0, 0)) {
				crypt->options.iv = crypt::IV::zero;
			} else {
				crypt->options.iv = crypt::IV::custom;
				crypt::UserData data;
				if (!checkCustomIV(data, true)) {
					throw CExc::CExc(CExc::Code::invalid_iv);
				} else {
					ivdata->set(data.BytePtr(), data.size());
				}
			}
		}

		// ------- auth
		if (operation == Operation::Enc) {
			crypt->hmac.enable = (::SendDlgItemMessage(tab.auth, IDC_CRYPT_AUTH_ENABLE, BM_GETCHECK, 0, 0) ? true : false);
			if (crypt->hmac.enable) {
				crypt->hmac.hash.algorithm = crypt::help::getHashByIndex(::SendDlgItemMessage(tab.auth, IDC_CRYPT_AUTH_HASH, CB_GETCURSEL, 0, 0), crypt::HMAC_SUPPORT);
				crypt->hmac.hash.digest_length = crypt::help::getHashDigestByIndex(crypt->hmac.hash.algorithm, ::SendDlgItemMessage(tab.auth, IDC_CRYPT_AUTH_HASH_LENGTH, CB_GETCURSEL, 0, 0));
				crypt->hmac.hash.encoding = crypt::Encoding::base64;
				crypt->hmac.hash.use_key = true;
				if (::SendDlgItemMessage(tab.auth, IDC_CRYPT_AUTH_KEY_PRESET, BM_GETCHECK, 0, 0)) {
					crypt->hmac.keypreset_id = (int)::SendDlgItemMessage(tab.auth, IDC_CRYPT_AUTH_KEY_LIST, CB_GETCURSEL, 0, 0);
					crypt->hmac.hash.key.set(preferences.getKey((size_t)crypt->hmac.keypreset_id), 16);
				} else {
					crypt->hmac.keypreset_id = -1;
				}
			}
		}

		// ------- password
		crypt::Encoding pw_encoding = (crypt::Encoding)::SendDlgItemMessage(tab.basic, IDC_CRYPT_PASSWORD_ENC, CB_GETCURSEL, 0, 0);
		crypt->options.password.set(current.password.c_str(), current.password.size(), pw_encoding);
		current.password.clear();
	}
	catch (CExc& exc) {
		helper::Windows::error(_hSelf, exc.what());
		return false;
	}
	return true;
}

bool DlgCrypt::OnClickOK()
{
	// custom hmac-key valid?
	if (operation == Operation::Enc && !confirm_password && !!::SendDlgItemMessage(tab.auth, IDC_CRYPT_AUTH_ENABLE, BM_GETCHECK, 0, 0)
		&& !!::SendDlgItemMessage(tab.auth, IDC_CRYPT_AUTH_KEY_CUSTOM, BM_GETCHECK, 0, 0)) {
		invalid.hmac_key = !checkHMACKey(crypt->hmac.hash.key, true);
		if (invalid.hmac_key) {
			TabCtrl_SetCurSel(::GetDlgItem(_hSelf, IDC_CRYPT_TAB), 4);
			changeActiveTab(4);
			InvalidateRect(::GetDlgItem(tab.auth, IDC_CRYPT_AUTH_PW_VALUE), NULL, NULL);
			::SetFocus(::GetDlgItem(tab.auth, IDC_CRYPT_AUTH_PW_VALUE));
			return false;
		}
	}
	// custom iv valid?
	if (((operation == Operation::Enc && !confirm_password) || operation == Operation::Dec)
		&& ::IsWindowEnabled(::GetDlgItem(tab.iv, IDC_CRYPT_IV_CUSTOM))
		&& !!::SendDlgItemMessage(tab.iv, IDC_CRYPT_IV_CUSTOM, BM_GETCHECK, 0, 0)) {
		crypt::UserData ivdata;
		invalid.iv = !checkCustomIV(ivdata, true);
		if (invalid.iv) {
			TabCtrl_SetCurSel(::GetDlgItem(_hSelf, IDC_CRYPT_TAB), 3);
			changeActiveTab(3);
			InvalidateRect(::GetDlgItem(tab.iv, IDC_CRYPT_IV_INPUT), NULL, NULL);
			::SetFocus(::GetDlgItem(tab.iv, IDC_CRYPT_IV_INPUT));
			return false;
		}
	}

	// get password
	crypt::secure_string temp_pw_str;
	crypt::Encoding password_enc = (crypt::Encoding)::SendDlgItemMessage(tab.basic, IDC_CRYPT_PASSWORD_ENC, CB_GETCURSEL, 0, 0);
	if (!checkPassword(temp_pw_str, true)) {
		::SetFocus(::GetDlgItem(tab.basic, IDC_CRYPT_PASSWORD));
		return false;
	}

	if (operation == Operation::Enc && !confirm_password) {
		// confirmation needed
		current.password.assign(temp_pw_str);
		::SetDlgItemText(tab.basic, IDC_CRYPT_PASSWORD_STATIC, TEXT("confirm:"));
		if (password_enc == crypt::Encoding::ascii) {
			// no password encoding: user has to reenter password for confirmation
			::SetDlgItemText(tab.basic, IDC_CRYPT_PASSWORD, TEXT(""));
			::EnableWindow(::GetDlgItem(tab.basic, IDC_CRYPT_PASSWORD_ENC), false);
			::SetFocus(::GetDlgItem(tab.basic, IDC_CRYPT_PASSWORD));
		} else {
			// encoded password: present reencoded password to user for confirmation
			setText(IDC_CRYPT_PASSWORD, current.password, tab.basic);
			::EnableWindow(::GetDlgItem(tab.basic, IDC_CRYPT_PASSWORD), false);
			::EnableWindow(::GetDlgItem(tab.basic, IDC_CRYPT_PASSWORD_ENC), false);
		}
		confirm_password = true;
	} else {
		if (operation == Operation::Enc) {
			// encryption: confirm password
			if (current.password.compare(temp_pw_str) == 0) {
				if (prepareOptions()) {
					return true;
				}
			} else {
				::SetDlgItemText(tab.basic, IDC_CRYPT_PASSWORD_STATIC, TEXT("password:"));
				::SetDlgItemText(tab.basic, IDC_CRYPT_PASSWORD, TEXT(""));
				::EnableWindow(::GetDlgItem(tab.basic, IDC_CRYPT_PASSWORD_ENC), true);
				::SetFocus(::GetDlgItem(tab.basic, IDC_CRYPT_PASSWORD));
				confirm_password = false;
			}
		} else {
			// decryption
			current.password.assign(temp_pw_str);
			if (prepareOptions()) {
				return true;
			}
		}
	}
	return false;
}

void DlgCrypt::checkSpinControlValue(int ctrlID)
{
	int		edit_id = -1;
	int		spin_id, vmin, vmax;
	HWND	hwnd;

	switch (ctrlID)
	{
	case IDC_CRYPT_ENC_LINELEN:
	{
		edit_id = IDC_CRYPT_ENC_LINELEN; spin_id = IDC_CRYPT_ENC_LINELEN_SPIN;
		vmin = 1; vmax = NPPC_MAX_LINE_LENGTH;
		hwnd = tab.encoding;
		break;
	}
	case IDC_CRYPT_SALT_BYTES:
	{
		edit_id = IDC_CRYPT_SALT_BYTES; spin_id = IDC_CRYPT_SALT_SPIN;
		vmin = 1; vmax = crypt::Constants::salt_max;
		hwnd = tab.key;
		break;
	}
	case IDC_CRYPT_PBKDF2_ITER:
	{
		edit_id = IDC_CRYPT_PBKDF2_ITER; spin_id = IDC_CRYPT_PBKDF2_ITER_SPIN;
		vmin = crypt::Constants::pbkdf2_iter_min; vmax = crypt::Constants::pbkdf2_iter_max;
		hwnd = tab.key;
		break;
	}
	case IDC_CRYPT_BCRYPT_ITER:
	{
		edit_id = IDC_CRYPT_BCRYPT_ITER; spin_id = IDC_CRYPT_BCRYPT_ITER_SPIN;
		vmin = crypt::Constants::bcrypt_iter_min; vmax = crypt::Constants::bcrypt_iter_max;
		hwnd = tab.key;
		break;
	}
	case IDC_CRYPT_SCRYPT_N:
	{
		edit_id = IDC_CRYPT_SCRYPT_N; spin_id = IDC_CRYPT_SCRYPT_N_SPIN;
		vmin = crypt::Constants::scrypt_N_min; vmax = crypt::Constants::scrypt_N_max;
		hwnd = tab.key;
		break;
	}
	case IDC_CRYPT_SCRYPT_R:
	{
		edit_id = IDC_CRYPT_SCRYPT_R; spin_id = IDC_CRYPT_SCRYPT_R_SPIN;
		vmin = crypt::Constants::scrypt_r_min; vmax = crypt::Constants::scrypt_r_max;
		hwnd = tab.key;
		break;
	}
	case IDC_CRYPT_SCRYPT_P:
	{
		edit_id = IDC_CRYPT_SCRYPT_P; spin_id = IDC_CRYPT_SCRYPT_P_SPIN;
		vmin = crypt::Constants::scrypt_p_min; vmax = crypt::Constants::scrypt_p_max;
		hwnd = tab.key;
		break;
	}
	}
	if (edit_id != -1) {
		int temp;
		int len = GetWindowTextLength(::GetDlgItem(hwnd, edit_id));
		if (len > 0) {
			std::wstring temp_str;
			temp_str.resize(len + 1);
			::GetDlgItemText(hwnd, edit_id, &temp_str[0], (int)temp_str.size());
			if (temp_str.size()) {
				temp = std::stoi(temp_str.c_str());
				if (temp > vmax) {
					::SendDlgItemMessage(hwnd, spin_id, UDM_SETPOS32, 0, vmax);
				} else if (temp < vmin) {
					::SendDlgItemMessage(hwnd, spin_id, UDM_SETPOS32, 0, vmin);
				}
			}
		} else {
			::SendDlgItemMessage(hwnd, spin_id, UDM_SETPOS32, 0, vmin);
		}
	}
}

bool DlgCrypt::checkPassword(crypt::secure_string& s, bool strict)
{
	getText(IDC_CRYPT_PASSWORD, s, tab.basic);
	size_t slen = s.size();
	crypt::Encoding enc = (crypt::Encoding)::SendDlgItemMessage(tab.basic, IDC_CRYPT_PASSWORD_ENC, CB_GETCURSEL, 0, 0);
	if (enc != crypt::Encoding::ascii) {
		crypt::UserData data(s.c_str(), enc);
		data.get(s, enc);
	}
	if (s.size() || (slen == 0 && !strict)) {
		if (invalid.password) {
			invalid.password = false;
			InvalidateRect(::GetDlgItem(tab.basic, IDC_CRYPT_PASSWORD), NULL, NULL);
		}
	} else {
		if (!invalid.password) {
			if (slen || strict) {
				invalid.password = true;
				InvalidateRect(::GetDlgItem(tab.basic, IDC_CRYPT_PASSWORD), NULL, NULL);
			}
		}
	}
	return !invalid.password;
}

bool DlgCrypt::checkCustomIV(crypt::UserData& data, bool reencode)
{
	crypt::secure_string temp;
	getText(IDC_CRYPT_IV_INPUT, temp, tab.iv);
	if (temp.size()) {
		crypt::Encoding enc = (crypt::Encoding)::SendDlgItemMessage(tab.iv, IDC_CRYPT_IV_ENC, CB_GETCURSEL, 0, 0);
		if (enc != crypt::Encoding::ascii) {
			data.set(temp.c_str(), temp.size(), enc);
			if (reencode && data.size() == current.iv_length) {
				data.get(temp, enc);
				setText(IDC_CRYPT_IV_INPUT, temp, tab.iv);
			}
		} else {
			data.set((const byte*)temp.c_str(), temp.size());
		}
	} else {
		data.clear();
	}
	return (data.size() == current.iv_length);
}

bool DlgCrypt::checkHMACKey(crypt::UserData& data, bool reencode)
{
	crypt::secure_string temp;
	getText(IDC_CRYPT_AUTH_PW_VALUE, temp, tab.auth);
	if (temp.size()) {
		crypt::Encoding enc = (crypt::Encoding)::SendDlgItemMessage(tab.auth, IDC_CRYPT_AUTH_PW_ENC, CB_GETCURSEL, 0, 0);
		if (enc != crypt::Encoding::ascii) {
			data.set(temp.c_str(), temp.size(), enc);
			if (reencode && data.size() > 0) {
				data.get(temp, enc);
				setText(IDC_CRYPT_AUTH_PW_VALUE, temp, tab.auth);
			}
		} else {
			data.set((const byte*)temp.c_str(), temp.size());
		}
		return (data.size() > 0);
	} else {
		data.clear();
		return false;
	}
}

void DlgCrypt::updateCipherInfo(crypt::Cipher cipher, crypt::Mode mode)
{
	// update CipherInfo Editbox
	size_t key_length, block_size;
	crypt::getCipherInfo(cipher, mode, key_length, current.iv_length, block_size);
	std::wostringstream s1;
	if (block_size == 0) {
		s1 << TEXT("IV: ") << std::to_wstring(current.iv_length * 8) << TEXT(" Bit");
	} else {
		s1 << TEXT("Blocksize: ") << std::to_wstring(block_size * 8) << TEXT(" Bit");
	}	
	::SetDlgItemText(tab.basic, IDC_CRYPT_CIPHER_INFO, s1.str().c_str());
	// update IV custom Radiobutton to show required length
	std::wostringstream s2;
	s2 << TEXT("custom (") << std::to_wstring(current.iv_length) << TEXT(" Bytes):");
	::SetDlgItemText(tab.iv, IDC_CRYPT_IV_CUSTOM, s2.str().c_str());
}

void DlgCrypt::updateCipherModeInfo(crypt::Mode mode)
{
	bool isECB = (mode == crypt::Mode::ecb);

	help.mode.setURL(crypt::help::getHelpURL(mode));
	help.mode.setTooltip(crypt::help::getInfo(mode));
	help.mode.setWarning(isECB);	
	updateIVControls(isECB);
}

void DlgCrypt::updateIVControls(bool disable)
{
	if (operation == Operation::Dec) {
		::EnableWindow(::GetDlgItem(tab.iv, IDC_CRYPT_IV_RANDOM), false);
	} else {
		::EnableWindow(::GetDlgItem(tab.iv, IDC_CRYPT_IV_RANDOM), !disable);
	}	
	::EnableWindow(::GetDlgItem(tab.iv, IDC_CRYPT_IV_KEY), !disable);
	::EnableWindow(::GetDlgItem(tab.iv, IDC_CRYPT_IV_ZERO), !disable);
	::EnableWindow(::GetDlgItem(tab.iv, IDC_CRYPT_IV_CUSTOM), !disable);
	if (disable) {
		help.iv.setTooltip("No IV needed (ECB-Mode)");
		::EnableWindow(::GetDlgItem(tab.iv, IDC_CRYPT_IV_INPUT), false);
		::EnableWindow(::GetDlgItem(tab.iv, IDC_CRYPT_IV_ENC), false);
	} else {
		bool enableInput = false;
		if (::SendDlgItemMessage(tab.iv, IDC_CRYPT_IV_CUSTOM, BM_GETCHECK, 0, 0)) {
			help.iv.setTooltip(crypt::help::getInfo(crypt::IV::custom));
			enableInput = true;
		} else if (::SendDlgItemMessage(tab.iv, IDC_CRYPT_IV_ZERO, BM_GETCHECK, 0, 0)) {
			help.iv.setTooltip(crypt::help::getInfo(crypt::IV::zero));
		} else if (::SendDlgItemMessage(tab.iv, IDC_CRYPT_IV_KEY, BM_GETCHECK, 0, 0)) {
			help.iv.setTooltip(crypt::help::getInfo(crypt::IV::keyderivation));
		} else {
			help.iv.setTooltip(crypt::help::getInfo(crypt::IV::random));
		}
		::EnableWindow(::GetDlgItem(tab.iv, IDC_CRYPT_IV_INPUT), enableInput);
		::EnableWindow(::GetDlgItem(tab.iv, IDC_CRYPT_IV_ENC), enableInput);
	}
}

void DlgCrypt::updateEncodingControls(crypt::Encoding enc)
{
	help.encoding.setURL(crypt::help::getHelpURL(enc));
	if (enc == crypt::Encoding::ascii) {
		help.encoding.setWarning(true);
		help.encoding.setTooltip(crypt::help::getInfo(crypt::Encoding::ascii));
	} else {
		help.encoding.setWarning(false);
		help.encoding.setTooltip(crypt::help::getInfo(enc));
	}

	if (operation != Operation::Enc) {
		return;
	}

	using namespace crypt;
	switch (enc)
	{
	case Encoding::ascii:
	{
		::EnableWindow(::GetDlgItem(tab.encoding, IDC_CRYPT_ENC_LINEBREAK), false);
		::EnableWindow(::GetDlgItem(tab.encoding, IDC_CRYPT_ENC_LB_WIN), false);
		::EnableWindow(::GetDlgItem(tab.encoding, IDC_CRYPT_ENC_LB_UNIX), false);
		::EnableWindow(::GetDlgItem(tab.encoding, IDC_CRYPT_ENC_LINELEN), false);
		::EnableWindow(::GetDlgItem(tab.encoding, IDC_CRYPT_ENC_LINELEN_SPIN), false);
		::EnableWindow(::GetDlgItem(tab.encoding, IDC_CRYPT_ENC_UPPERCASE), false);
		::EnableWindow(::GetDlgItem(tab.encoding, IDC_CRYPT_ENC_STATIC), false);
		break;
	}
	case Encoding::base16: case Encoding::base32: case Encoding::base64:
	{
		::EnableWindow(::GetDlgItem(tab.encoding, IDC_CRYPT_ENC_LINEBREAK), true);
		bool linebreaks = !!::SendDlgItemMessage(tab.encoding, IDC_CRYPT_ENC_LINEBREAK, BM_GETCHECK, 0, 0);
		::EnableWindow(::GetDlgItem(tab.encoding, IDC_CRYPT_ENC_LB_WIN), linebreaks);
		::EnableWindow(::GetDlgItem(tab.encoding, IDC_CRYPT_ENC_LB_UNIX), linebreaks);
		::EnableWindow(::GetDlgItem(tab.encoding, IDC_CRYPT_ENC_LINELEN), linebreaks);
		::EnableWindow(::GetDlgItem(tab.encoding, IDC_CRYPT_ENC_LINELEN_SPIN), linebreaks);
		::EnableWindow(::GetDlgItem(tab.encoding, IDC_CRYPT_ENC_STATIC), linebreaks);
		::EnableWindow(::GetDlgItem(tab.encoding, IDC_CRYPT_ENC_UPPERCASE), (enc != Encoding::base64));
		break;
	}
	}
}

void DlgCrypt::updateHashDigestControl(crypt::Hash h, HWND hwnd, int id)
{
	std::wstring temp_str;
	::SendDlgItemMessage(hwnd, id, CB_RESETCONTENT, 0, 0);
	for (crypt::help::HashDigests it(h); *it; ++it) {
		temp_str = std::to_wstring(*it * 8);
		temp_str.append(TEXT(" Bits"));
		::SendDlgItemMessage(hwnd, id, CB_ADDSTRING, 0, (LPARAM)temp_str.c_str());
	}
}

void DlgCrypt::updateKeyDerivationControls()
{
	switch (current.key_derivation)
	{
	case crypt::KeyDerivation::pbkdf2:
	{
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_PBKDF2_HASH), true);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_PBKDF2_HASH_LENGTH), true);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_PBKDF2_ITER), true);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_PBKDF2_ITER_SPIN), true);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_PBKDF2_STATIC1), true);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_PBKDF2_STATIC3), true);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_BCRYPT_STATIC1), false);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_BCRYPT_STATIC2), false);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_BCRYPT_ITER), false);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_BCRYPT_ITER_SPIN), false);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_SCRYPT_STATIC1), false);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_SCRYPT_STATIC2), false);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_SCRYPT_STATIC3), false);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_SCRYPT_N), false);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_SCRYPT_N_SPIN), false);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_SCRYPT_R), false);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_SCRYPT_R_SPIN), false);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_SCRYPT_P), false);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_SCRYPT_P_SPIN), false);
		// the salt-bytes edit may have got deactivated because bcrypt was chosen:
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_SALT), true);
		bool c = !!::SendDlgItemMessage(tab.key, IDC_CRYPT_SALT, BM_GETCHECK, 0, 0);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_SALT_BYTES), c);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_SALT_SPIN), c);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_SALT_STATIC), c);
		break;
	}
	case crypt::KeyDerivation::bcrypt:
	{
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_PBKDF2_HASH), false);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_PBKDF2_HASH_LENGTH), false);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_PBKDF2_ITER), false);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_PBKDF2_ITER_SPIN), false);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_PBKDF2_STATIC1), false);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_PBKDF2_STATIC3), false);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_BCRYPT_STATIC1), true);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_BCRYPT_STATIC2), true);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_BCRYPT_ITER), true);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_BCRYPT_ITER_SPIN), true);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_SCRYPT_STATIC1), false);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_SCRYPT_STATIC2), false);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_SCRYPT_STATIC3), false);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_SCRYPT_N), false);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_SCRYPT_N_SPIN), false);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_SCRYPT_R), false);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_SCRYPT_R_SPIN), false);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_SCRYPT_P), false);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_SCRYPT_P_SPIN), false);
		// bcrypt requires 16 bytes salt:
		::SetDlgItemInt(tab.key, IDC_CRYPT_SALT_BYTES, 16, false);
		::SendDlgItemMessage(tab.key, IDC_CRYPT_SALT, BM_SETCHECK, TRUE, 0);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_SALT), false);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_SALT_BYTES), false);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_SALT_SPIN), false);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_SALT_STATIC), false);
		break;
	}
	case crypt::KeyDerivation::scrypt:
	{
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_PBKDF2_HASH), false);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_PBKDF2_HASH_LENGTH), false);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_PBKDF2_ITER), false);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_PBKDF2_ITER_SPIN), false);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_PBKDF2_STATIC1), false);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_PBKDF2_STATIC3), false);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_PBKDF2_HASH), false);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_PBKDF2_ITER), false);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_PBKDF2_ITER_SPIN), false);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_BCRYPT_STATIC1), false);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_BCRYPT_STATIC2), false);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_BCRYPT_ITER), false);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_BCRYPT_ITER_SPIN), false);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_SCRYPT_STATIC1), true);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_SCRYPT_STATIC2), true);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_SCRYPT_STATIC3), true);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_SCRYPT_N), true);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_SCRYPT_N_SPIN), true);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_SCRYPT_R), true);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_SCRYPT_R_SPIN), true);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_SCRYPT_P), true);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_SCRYPT_P_SPIN), true);
		// the salt-bytes edit may have got deactivated because bcrypt was chosen:
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_SALT), true);
		bool c = !!::SendDlgItemMessage(tab.key, IDC_CRYPT_SALT, BM_GETCHECK, 0, 0);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_SALT_BYTES), c);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_SALT_SPIN), c);
		::EnableWindow(::GetDlgItem(tab.key, IDC_CRYPT_SALT_STATIC), c);
		break;
	}
	}
	help.keyalgorithm.setURL(crypt::help::getHelpURL(current.key_derivation));
	help.keyalgorithm.setTooltip(crypt::help::getInfo(current.key_derivation));
}

void DlgCrypt::updateCipherControls()
{
	crypt::Mode old_mode = crypt::help::getModeByIndex(current.cipher, (int)::SendDlgItemMessage(tab.basic, IDC_CRYPT_MODE, CB_GETCURSEL, 0, 0));
	crypt::Mode new_mode = old_mode;

	std::wstring temp_str;
	int cipher_cat = (int)::SendDlgItemMessage(tab.basic, IDC_CRYPT_CIPHER_TYPE, CB_GETCURSEL, 0, 0);
	int cipher_index = (int)::SendDlgItemMessage(tab.basic, IDC_CRYPT_CIPHER, CB_GETCURSEL, 0, 0);

	current.cipher = crypt::help::getCipherByIndex(cipher_cat, cipher_index);
	// refill combobox with the keylengths available for the current cipher:
	::SendDlgItemMessage(tab.basic, IDC_CRYPT_KEYLENGTH, CB_RESETCONTENT, 0, 0);

	int keylength_index = 0;
	int i = 0;
	for (crypt::help::CipherKeys it(current.cipher); *it; ++it, i++) {
		if (current.key_length == (size_t)*it) {
			keylength_index = i;
		}
		temp_str = std::to_wstring(*it * 8);
		temp_str.append(TEXT(" Bits"));
		::SendDlgItemMessage(tab.basic, IDC_CRYPT_KEYLENGTH, CB_ADDSTRING, 0, (LPARAM)temp_str.c_str());
	}
	::SendDlgItemMessage(tab.basic, IDC_CRYPT_KEYLENGTH, CB_SETCURSEL, keylength_index, 0);
	current.key_length = crypt::help::getCipherKeylengthByIndex(current.cipher, keylength_index);

	// refill combobox with the modes available for the current cipher:
	::SendDlgItemMessage(tab.basic, IDC_CRYPT_MODE, CB_RESETCONTENT, 0, 0);
	for(crypt::help::CipherModes it(current.cipher); *it; ++it) {
		::SendDlgItemMessage(tab.basic, IDC_CRYPT_MODE, CB_ADDSTRING, 0, (LPARAM)helper::Windows::ToWCHAR(*it).c_str());
	}

	int cur_mode_count = (int)::SendDlgItemMessage(tab.basic, IDC_CRYPT_MODE, CB_GETCOUNT, 0, 0);
	if (cur_mode_count == 0) {
		::EnableWindow(::GetDlgItem(tab.basic, IDC_CRYPT_MODE), false);
		help.mode.enable(false);
	} else {
		::EnableWindow(::GetDlgItem(tab.basic, IDC_CRYPT_MODE), true);
		help.mode.enable(true);
		// check if the current cipher supports the old mode:
		int i = crypt::help::getModeIndex(current.cipher, old_mode);
		if (i != -1) {
			::SendDlgItemMessage(tab.basic, IDC_CRYPT_MODE, CB_SETCURSEL, i, 0);
		} else {
			new_mode = crypt::help::getModeByIndex(current.cipher, 0);
			updateCipherModeInfo(new_mode);
			::SendDlgItemMessage(tab.basic, IDC_CRYPT_MODE, CB_SETCURSEL, 0, 0);
		}		
	}
	help.cipher.setURL(crypt::help::getHelpURL(current.cipher));
	help.cipher.setTooltip(crypt::help::getInfo(current.cipher));
	if (crypt::help::checkProperty(current.cipher, crypt::WEAK)) {
		help.cipher.setWarning(true);
	} else {
		help.cipher.setWarning(false);
	}	

	updateCipherInfo(current.cipher, new_mode);
	PostMessage(tab.basic, WM_NEXTDLGCTL, (WPARAM)::GetDlgItem(tab.basic, IDC_CRYPT_PASSWORD), TRUE);
}


