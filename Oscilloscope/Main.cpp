#include "GacLib\GacUI.h"
#include "GacLib\GacUIWindows.h"

#pragma warning(disable:4244)

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int CmdShow)
{
	return SetupWindowsDirect2DRenderer();
}

class OscilloscopeMainWindow : public GuiWindow
{
private:
	IStyleController*		m_defaultStyleController;
	GuiToolstripMenuBar*	m_menuBar;
	GuiDirect2DElement*		m_OscilloscopeScreen;

	GuiToolstripButton*		m_ButtonStart;
	ComPtr<ID2D1SolidColorBrush> m_TestBrush;

public:
	OscilloscopeMainWindow()
		: GuiWindow(m_defaultStyleController = GetCurrentTheme()->CreateWindowStyle())
	{
		this->SetText(L"Oscilloscope V0.01");
		this->SetClientSize(Size(800, 600));

		GuiTableComposition* table = new GuiTableComposition;
		table->SetCellPadding(0);
		table->SetAlignmentToParent(Margin(0, 0, 0, 0));
		table->SetRowsAndColumns(2, 2);
		table->SetRowOption(0, GuiCellOption::MinSizeOption());
		table->SetRowOption(1, GuiCellOption::PercentageOption(1.0));
		table->SetColumnOption(0, GuiCellOption::MinSizeOption());
		table->SetColumnOption(1, GuiCellOption::PercentageOption(1.0));
		this->GetContainerComposition()->AddChild(table);

		{
			GuiCellComposition* cell = new GuiCellComposition;
			table->AddChild(cell);
			cell->SetSite(0, 0, 1, 2);
			m_menuBar = g::NewMenuBar();
			m_menuBar->GetBoundsComposition()->SetMinSizeLimitation(GuiGraphicsComposition::LimitToElementAndChildren);
			m_menuBar->GetBoundsComposition()->SetAlignmentToParent(Margin(0, 0, 0, 0));

			// set menu
			m_menuBar->GetBuilder()
				->Button(0, L"File")
				->BeginSubMenu()
				->Splitter()
				->EndSubMenu()
				->Button(0, L"View")
				->BeginSubMenu()
				->Splitter()
				->Splitter()
				->EndSubMenu();
			cell->AddChild(m_menuBar->GetBoundsComposition());
		}

		// Display
		{
			GuiCellComposition* cell = new GuiCellComposition;
			table->AddChild(cell);
			cell->SetSite(1, 0, 1, 1);
			cell->SetInternalMargin(Margin(1, 0, 1, 0));
			cell->SetMinSizeLimitation(GuiGraphicsComposition::LimitToElementAndChildren);
			
			GuiDirect2DElement* element = GuiDirect2DElement::Create();
			element->Rendering.AttachMethod(this, &OscilloscopeMainWindow::OnRendering);
			element->BeforeRenderTargetChanged.AttachMethod(this, &OscilloscopeMainWindow::OnRenderTargetLost);
			element->AfterRenderTargetChanged.AttachMethod(this, &OscilloscopeMainWindow::OnRenderTargetGet);

			GuiBoundsComposition* composition = new GuiBoundsComposition;
			composition->SetAlignmentToParent(Margin(0, 0, 0, 0));
			composition->SetBounds(Rect(0, 0, 640, 640));
			composition->SetOwnedElement(element);
			cell->AddChild(composition);
		}

		// Controll Panel
		{
			GuiCellComposition* cell = new GuiCellComposition;
			table->AddChild(cell);
			cell->SetSite(1, 1, 1, 1);
			cell->SetInternalMargin(Margin(1, 0, 1, 0));

			//GuiTableComposition* table = new GuiTableComposition;
			m_ButtonStart = g::NewToolBarButton();
			m_ButtonStart->SetText(L"Start");
			m_ButtonStart->SetAutoSelection(true);
			GuiBoundsComposition* buttonComposition = m_ButtonStart->GetBoundsComposition();
			buttonComposition->SetAlignmentToParent(Margin(5, 5, 5, 5));
			cell->AddChild(m_ButtonStart->GetBoundsComposition());
		}
	}

	void OnRendering(GuiGraphicsComposition* sender, GuiDirect2DElementEventArgs& arguments)
	{
		int SurfaceWidth = arguments.bounds.Width();
		int SurfaceHeight = arguments.bounds.Height();
		int SurfaceX = arguments.bounds.Left();
		int SurfaceY = arguments.bounds.Top();

		ID2D1RenderTarget* renderTarget = arguments.rt;
		renderTarget->Clear(D2D1::ColorF(0, 0, 0));
		float strokeWidth = 0.5f;
		// Draw Background Grid
		{
			renderTarget->DrawLine(D2D1::Point2F(SurfaceWidth / 2 + SurfaceX, 0 + SurfaceY), D2D1::Point2F(SurfaceWidth / 2 + SurfaceX, SurfaceHeight + SurfaceY), m_TestBrush.Obj(), strokeWidth);
			
			float Y = SurfaceY;
			for (int i = 0; i < 100; i++)
			{
				if (i == 50)
				{
					renderTarget->DrawLine(D2D1::Point2F(0 + SurfaceX, Y), D2D1::Point2F(SurfaceWidth + SurfaceX, Y), m_TestBrush.Obj(), strokeWidth);
				}
				else if (i % 10 == 0)
				{
					renderTarget->DrawLine(D2D1::Point2F(SurfaceWidth / 2 + SurfaceX, Y), D2D1::Point2F(SurfaceWidth / 2 + SurfaceX + 10, Y), m_TestBrush.Obj(), strokeWidth);
				}
				else
				{
					renderTarget->DrawLine(D2D1::Point2F(SurfaceWidth / 2 + SurfaceX, Y), D2D1::Point2F(SurfaceWidth / 2 + SurfaceX + 3, Y), m_TestBrush.Obj(), strokeWidth);
				}
				Y += SurfaceHeight / 100.0f;
			}
		}
	}

	void OnRenderTargetGet(GuiGraphicsComposition* sender, GuiDirect2DElementEventArgs& arguments)
	{
		ID2D1SolidColorBrush* brush;
		arguments.rt->CreateSolidColorBrush(D2D1::ColorF(0.0f, 1.0f, 0.0f), D2D1::BrushProperties(), &brush);
		m_TestBrush = brush;
	}

	void OnRenderTargetLost(GuiGraphicsComposition* sender, GuiDirect2DElementEventArgs& arguments)
	{
		m_TestBrush = NULL;
	}
};

void GuiMain()
{
	GuiWindow* window = new OscilloscopeMainWindow;
	GetApplication()->Run(window);
	delete window;
}