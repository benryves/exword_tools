/* ExwordTextLoader - Main TextLoader frame
 *
 * Copyright (C) 2011 - Brian Johnson <brijohn@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <wx/dnd.h>

#include "TextLoaderFrame.h"
#include "UploadThread.h"

class ExwordFileDropTarget : public wxFileDropTarget
{
    public:
        ExwordFileDropTarget(TextLoaderFrame *frame) { m_frame = frame; };
        virtual bool OnDropFiles(wxCoord x, wxCoord y, const wxArrayString& filenames) {
            wxArrayString list;
            for (int i = 0; i < filenames.GetCount(); ++i) {
                wxFileName filename(filenames[i]);
                if (filename.GetExt().IsSameAs(wxT("txt"), false)) {
                    if (filename.IsFileReadable())
                        list.Add(filenames[i]);
                }
            }
            m_frame->UploadFiles(list);
            return true;
        }
    private:
        TextLoaderFrame *m_frame;
};

BEGIN_EVENT_TABLE(TextLoaderFrame, TextLoaderGUI)
    EVT_BUTTON(XRCID("m_connect"), TextLoaderFrame::OnConnect)
    EVT_BUTTON(XRCID("m_delete"), TextLoaderFrame::OnDelete)
    EVT_RADIOBUTTON(XRCID("m_internal"), TextLoaderFrame::OnInternal)
    EVT_RADIOBUTTON(XRCID("m_sd"), TextLoaderFrame::OnSDCard)
    EVT_COMMAND(myID_START, myEVT_THREAD, TextLoaderFrame::OnThreadStart)
    EVT_COMMAND(myID_FINISH, myEVT_THREAD, TextLoaderFrame::OnThreadFinish)
    EVT_DISCONNECT(TextLoaderFrame::OnDisconnect)
    EVT_FILE_TRANSFER(TextLoaderFrame::OnTransfer)
END_EVENT_TABLE()

TextLoaderFrame::TextLoaderFrame()
{
    int fieldWidths[] = {175, -1};
    m_sd->Enable(false);
    m_status->SetFieldsCount(2, fieldWidths);
    m_filelist->InsertColumn(0, _("Filename"));
    m_filelist->SetColumnWidth(0, 485);
    m_filelist->SetFont(wxFont(8, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, wxEmptyString));
    m_filelist->SetDropTarget(new ExwordFileDropTarget(this));
    m_exword.SetEventTarget(this);
    m_progress = new ProgressDialog(this, wxT(""));
}

TextLoaderFrame::~TextLoaderFrame()
{
    m_exword.SetEventTarget(NULL);
    m_exword.Disconnect();
    if (m_progress) {
        m_progress->Destroy();
        m_progress = NULL;
    }
}

ExwordRegion TextLoaderFrame::GetRegionFromString(wxString name)
{
    if (name == wxT("Japanese"))
        return JAPANESE;
    else if(name == wxT("Chinese"))
        return CHINESE;
    else if(name == wxT("Indian"))
        return INDIAN;
    else if(name == wxT("Korean"))
        return KOREAN;
    else if(name == wxT("Italian"))
        return ITALIAN;
    else if(name == wxT("German"))
        return GERMAN;
    else if(name == wxT("Spanish"))
        return SPANISH;
    else if(name == wxT("French"))
        return FRENCH;
    else if(name == wxT("Russian"))
        return RUSSIAN;
    return JAPANESE;
}

void TextLoaderFrame::UpdateStatusbar()
{
    wxString capTxt = wxT("");
    wxString modelTxt = wxT("");
    if (m_exword.IsConnected()) {
        Capacity cap = m_exword.GetCapacity();
        Model model = m_exword.GetModel();
        unsigned long percent = ((float)cap.GetFree() / (float)cap.GetTotal()) * 100;
        capTxt.Printf(_("Capacity: %lu / %lu (%lu%%)"), cap.GetFree(), cap.GetTotal(), percent);
        modelTxt.Printf(wxT("%s %s"), model.GetSeriesName().c_str(), model.GetModel().c_str());
    }
    m_status->SetStatusText(modelTxt, 0);
    m_status->SetStatusText(capTxt, 1);
}

void TextLoaderFrame::UpdateFilelist()
{
    DirEnts files = m_exword.List(wxT(""), wxT("*.TXT"));
    m_filelist->DeleteAllItems();
    for (unsigned int i = 0; i < files.GetCount(); ++i) {
        m_filelist->InsertItem(i, files[i].GetFilename());
        m_filelist->SetItemData(i, files[i].GetFlags());
    }
}

void TextLoaderFrame::UploadFiles(const wxArrayString& filenames)
{
    if (m_exword.IsConnected()) {
        UploadThread *thread = new UploadThread(this, &m_exword);
        if (!thread->Start(new wxArrayString(filenames)))
            wxMessageBox(_("Failed to start UploadThread"), _("Error"), wxOK, this);
    }
}

void TextLoaderFrame::OnConnect(wxCommandEvent& event)
{
    if (!m_exword.IsConnected()) {
        wxString region = m_region->GetString(m_region->GetCurrentSelection());
        if (!m_exword.Connect(TEXT, GetRegionFromString(region))) {
            wxMessageBox(_("Device not found"), _("Error"), wxOK, this);
            return;
        }
        m_region->Enable(false);
        if (m_exword.IsSdInserted())
            m_sd->Enable(true);
        m_connect->SetLabel(_("Disconnect"));
        UpdateFilelist();
    } else {
        m_region->Enable(true);
        m_sd->Enable(false);
        m_filelist->DeleteAllItems();
        m_internal->SetValue(true);
        m_connect->SetLabel(_("Connect"));
        m_exword.Disconnect();
    }
    UpdateStatusbar();
}

void TextLoaderFrame::OnDelete(wxCommandEvent& event)
{
    long item;
    wxString filename;
    unsigned long flags;
    item = m_filelist->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (item != -1) {
        filename = m_filelist->GetItemText(item);
        flags = m_filelist->GetItemData(item);
        m_exword.DeleteFile(filename, flags);
        m_filelist->DeleteItem(item);
        UpdateStatusbar();
    }
}

void TextLoaderFrame::OnInternal(wxCommandEvent& event)
{
    m_exword.SetStorage(INTERNAL);
    UpdateFilelist();
    UpdateStatusbar();
}

void TextLoaderFrame::OnSDCard(wxCommandEvent& event)
{
    m_exword.SetStorage(SD);
    UpdateFilelist();
    UpdateStatusbar();
}

void TextLoaderFrame::OnThreadStart(wxCommandEvent& event)
{
    m_progress->Update(0, event.GetString());
    m_progress->Show();
}

void TextLoaderFrame::OnTransfer(wxTransferEvent& event)
{
    unsigned long percent = ((float)event.GetTransferred() / (float)event.GetLength()) * 100;
    wxString text;
    text.Printf(_("Copying %s (%lu%%)"), event.GetFilename().c_str(), percent);
    if (m_progress->IsShown())
        m_progress->Update(percent, text);
}

void TextLoaderFrame::OnThreadFinish(wxCommandEvent& event)
{
    m_progress->Show(false);
    UpdateFilelist();
    UpdateStatusbar();
}

void TextLoaderFrame::OnDisconnect(wxCommandEvent& event)
{
    int reason = event.GetInt();
    m_region->Enable(true);
    m_sd->Enable(false);
    m_filelist->DeleteAllItems();
    m_internal->SetValue(true);
    m_connect->SetLabel(_("Connect"));
    m_exword.Disconnect();
    UpdateStatusbar();
}
