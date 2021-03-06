
#include "PicProcessor.h"
#include "PicProcessorBlackWhitePoint.h"
#include "PicProcPanel.h"
#include "FreeImage.h"
#include "undo.xpm"
//#include <omp.h>

#include "util.h"
#include "FreeImage_Threaded.h"
#include <wx/fileconf.h>

class BlackWhitePointPanel: public PicProcPanel
{
	public:
		BlackWhitePointPanel(wxPanel *parent, PicProcessor *proc, wxString params): PicProcPanel(parent, proc, params)
		{
			wxArrayString p = split(params,",");

			int blk = atoi(p[0]);
			int wht = atoi(p[1]);

			SetSize(parent->GetSize());
			wxSizerFlags flags = wxSizerFlags().Center().Border(wxLEFT|wxRIGHT|wxTOP|wxBOTTOM);
			g->Add(0,10, wxGBPosition(0,0));

			g->Add(new wxStaticText(this,wxID_ANY, "black: "), wxGBPosition(1,0), wxDefaultSpan, wxALIGN_LEFT | wxALL, 3);
			black = new wxSlider(this, wxID_ANY, blk, 0, 128, wxPoint(10, 30), wxSize(140, -1));
			g->Add(black , wxGBPosition(1,1), wxDefaultSpan, wxALIGN_LEFT | wxALL, 3);
			val1 = new wxStaticText(this,wxID_ANY, wxString::Format("%4d",blk), wxDefaultPosition, wxSize(30, -1));
			g->Add(val1 , wxGBPosition(1,2), wxDefaultSpan, wxALIGN_LEFT | wxALL, 3);
			btn1 = new wxBitmapButton(this, 1000, wxBitmap(undo_xpm), wxPoint(0,0), wxSize(-1,-1), wxBU_EXACTFIT);
			btn1->SetToolTip("Reset black point to default");
			g->Add(btn1, wxGBPosition(1,3), wxDefaultSpan, wxALIGN_LEFT | wxALL, 3);


			g->Add(new wxStaticText(this,wxID_ANY, "white: "), wxGBPosition(2,0), wxDefaultSpan, wxALIGN_LEFT | wxALL, 3);
			white = new wxSlider(this, wxID_ANY, wht, 128, 255, wxPoint(10, 30), wxSize(140, -1));
			g->Add(white , wxGBPosition(2,1), wxDefaultSpan, wxALIGN_LEFT | wxALL, 3);
			val2 = new wxStaticText(this,wxID_ANY, wxString::Format("%4d",wht), wxDefaultPosition, wxSize(30, -1));
			g->Add(val2 , wxGBPosition(2,2), wxDefaultSpan, wxALIGN_LEFT | wxALL, 3);
			btn2 = new wxBitmapButton(this, 2000, wxBitmap(undo_xpm), wxPoint(0,0), wxSize(-1,-1), wxBU_EXACTFIT);
			btn2->SetToolTip("Reset white point to default");
			g->Add(btn2, wxGBPosition(2,3), wxDefaultSpan, wxALIGN_LEFT | wxALL, 3);


			SetSizerAndFit(g);
			g->Layout();
			Refresh();
			Update();
			SetFocus();
			t = new wxTimer(this);
			Bind(wxEVT_BUTTON, &BlackWhitePointPanel::OnButton, this);
			Bind(wxEVT_SCROLL_CHANGED, &BlackWhitePointPanel::OnChanged, this);
			Bind(wxEVT_TIMER, &BlackWhitePointPanel::OnTimer,  this);
		}

		~BlackWhitePointPanel()
		{
			t->~wxTimer();
		}

		void OnChanged(wxCommandEvent& event)
		{
			val1->SetLabel(wxString::Format("%4d", black->GetValue()));
			val2->SetLabel(wxString::Format("%4d", white->GetValue()));
			t->Start(500,wxTIMER_ONE_SHOT);
		}

		void OnTimer(wxTimerEvent& event)
		{
			q->setParams(wxString::Format("%d,%d",black->GetValue(),white->GetValue()));
			q->processPic();
			event.Skip();
		}

		void OnButton(wxCommandEvent& event)
		{
			int resetblackval, resetwhiteval;
			switch (event.GetId()) {
				case 1000:
					wxConfigBase::Get()->Read("tool.blackwhitepoint.blackinitialvalue",&resetblackval,0);
					black->SetValue(resetblackval);
					val1->SetLabel(wxString::Format("%4d", resetblackval));
					break;
				case 2000:
					wxConfigBase::Get()->Read("tool.blackwhitepoint.whiteinitialvalue",&resetwhiteval,255);
					white->SetValue(resetwhiteval);
					val2->SetLabel(wxString::Format("%4d", resetwhiteval));
					break;
			}
			q->setParams(wxString::Format("%d,%d",black->GetValue(),white->GetValue()));
			q->processPic();
			event.Skip();
		}


	private:
		wxSlider *black, *white;
		wxStaticText *val1, *val2;
		wxBitmapButton *btn1, * btn2;
		wxTimer *t;

};


PicProcessorBlackWhitePoint::PicProcessorBlackWhitePoint(wxString name, wxString command, wxTreeCtrl *tree, PicPanel *display, wxPanel *parameters): PicProcessor(name, command,  tree, display, parameters) 
{
	int i;
	double blk, wht;
	DWORD hdata[256]; DWORD hmax=0;
	if (command == "") {
		FIBITMAP *hdib = FreeImage_ConvertTo24Bits(getPreviousPicProcessor()->getProcessedPic());
		FreeImage_GetHistogram(hdib, hdata);
		FreeImage_Unload(hdib);
		for (i=0; i<256; i++) if (hdata[i] > hmax) hmax = hdata[i];
		for (i=1; i<128; i++) if ((double)hdata[i]/(double)hmax > 0.05) break;
		blk = (double) i;
		for (i=255; i>=128; i--) if ((double)hdata[i]/(double)hmax > 0.05) break;
		wht = (double) i;
		setParams(wxString::Format("%d,%d",(unsigned) blk, (unsigned) wht));
	}
	else setParams(command);
	showParams();
	processPic();
}

void PicProcessorBlackWhitePoint::showParams()
{
	if (!m_parameters) return;
	m_parameters->DestroyChildren();
	r = new BlackWhitePointPanel(m_parameters, this, c);
}

bool PicProcessorBlackWhitePoint::processPic() {
	((wxFrame*) m_display->GetParent())->SetStatusText("black/white point...");

	wxArrayString cp = split(getParams(),",");
	double blk = atof(cp[0]);
	double wht = atof(cp[1]);

	Curve ctrlpts;
	ctrlpts.insertpoint(blk,0);
	ctrlpts.insertpoint(wht,255);
	
	bool result = true;

	int threadcount;
	wxConfigBase::Get()->Read("tool.blackwhitepoint.cores",&threadcount,0);
	if (threadcount == 0) threadcount = ThreadCount();

	mark();
	if (dib) FreeImage_Unload(dib);
	dib = FreeImage_Clone(getPreviousPicProcessor()->getProcessedPic());
	ApplyCurve(getPreviousPicProcessor()->getProcessedPic(), dib, ctrlpts.getControlPoints(), threadcount);
	wxString d = duration();

	if ((wxConfigBase::Get()->Read("tool.all.log","0") == "1") || (wxConfigBase::Get()->Read("tool.blackwhitepoint.log","0") == "1"))
		log(wxString::Format("tool=blackwhitepoint,imagesize=%dx%d,imagebpp=%d,threads=%d,time=%s",FreeImage_GetWidth(dib), FreeImage_GetHeight(dib),FreeImage_GetBPP(dib),threadcount,d));

	dirty=false;
	((wxFrame*) m_display->GetParent())->SetStatusText("");

	//put in every processPic()...
	if (m_tree->GetItemState(GetId()) == 1) m_display->SetPic(dib);
	wxTreeItemId next = m_tree->GetNextSibling(GetId());
	if (next.IsOk()) {
		PicProcessor * nextitem = (PicProcessor *) m_tree->GetItemData(next);
		nextitem->processPic();
	}

	return result;
}



