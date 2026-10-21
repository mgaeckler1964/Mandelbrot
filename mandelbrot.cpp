/*
		Project:		Mandelbrot
		Module:			mandelbrot.cpp
		Description:	Mandelbrot Generator
		Author:			Martin Gäckler
		Address:		Hofmannsthalweg 14, A-4030 Linz
		Web:			https://www.gaeckler.at/

		Copyright:		(c) 2026 Martin Gäckler

		This program is free software: you can redistribute it and/or modify
		it under the terms of the GNU General Public License as published by
		the Free Software Foundation, version 3.

		You should have received a copy of the GNU General Public License
		along with this program. If not, see <http://www.gnu.org/licenses/>.

		THIS SOFTWARE IS PROVIDED BY Martin Gäckler, Linz, Austria ``AS IS''
		AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
		TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
		PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR
		CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
		SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
		LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
		USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
		ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
		OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
		OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
		SUCH DAMAGE.
*/

// --------------------------------------------------------------------- //
// ----- switches ------------------------------------------------------ //
// --------------------------------------------------------------------- //

// --------------------------------------------------------------------- //
// ----- includes ------------------------------------------------------ //
// --------------------------------------------------------------------- //

#include <gak/fmtNumber.h>
#include <gak/gaklib.h>
#include <gak/math.h>
#include <gak/stopWatch.h>
#include <gak/threadPool.h>
#include <gak/xmlParser.h>

#include <WINLIB/WINAPP.H>
#include <winlib/POPUP.H>
#include <winlib/DEVICE.H>
#include <WINLIB/colors.h>
#include <WINLIB/StandardDialogs.h>

#include "mandelbrot_rc.h"

// --------------------------------------------------------------------- //
// ----- imported datas ------------------------------------------------ //
// --------------------------------------------------------------------- //

// --------------------------------------------------------------------- //
// ----- module switches ----------------------------------------------- //
// --------------------------------------------------------------------- //

#ifdef __BORLANDC__
#	pragma option -RT-
#	pragma option -b
#	pragma option -a4
#	pragma option -pc
#endif

using namespace winlib;

// --------------------------------------------------------------------- //
// ----- constants ----------------------------------------------------- //
// --------------------------------------------------------------------- //

typedef gak::math::Scale<double>	Scale;
typedef Scale::Pair					Range;
static const Range	X_RANGE(-2, 0.5),
					Y_RANGE(-1.25, 1.25);

static const int START_PALETTE = 0;
static const int MIN_ITERATIONS = 3;	// assertion failed if min is less than 3
static const int START_ITERATIONS = 128;
static const int ITERATION_STEP = 32;

#define START_COLOR_MODE cmRoot	// since cmRoot is not yet defined we use 
								//this preprocessor trick

static int LINE_COUNT = 1;

const unsigned RED = 0x01;
const unsigned GREEN = 0x02;
const unsigned BLUE = 0x04;

static const int COLORS[][3] = 
{
	{ RED, GREEN, BLUE },
	{ BLUE, RED, GREEN },
	{ GREEN, BLUE, RED },

	{ BLUE|GREEN, GREEN|RED, RED|BLUE },
	{ GREEN|RED, RED|BLUE, BLUE|GREEN },
	{ RED|BLUE, BLUE|GREEN, GREEN|RED },

	{ RED, BLUE, GREEN},
	{ BLUE, GREEN, RED },
	{ GREEN, RED, BLUE  },

	{ BLUE|GREEN, RED|BLUE, GREEN|RED },
	{ GREEN|RED, BLUE|GREEN, RED|BLUE },
	{ RED|BLUE, GREEN|RED, BLUE|GREEN },

	{ RED|GREEN|BLUE, RED|GREEN|BLUE, RED|GREEN|BLUE },
};

static const BGRA CENTER_COLORS[] =
{
	makeBGRA(colors::YELLOW),
	makeBGRA(colors::BLACK),
	makeBGRA(colors::GRAY),
	makeBGRA(colors::WHITE),
	makeBGRA(colors::BLUE),
	makeBGRA(colors::RED),
	makeBGRA(colors::LIME),
	makeBGRA(colors::MAGENTA),
	makeBGRA(colors::CYAN),
	makeBGRA(colors::MAROON),
	makeBGRA(colors::GREEN),
	makeBGRA(colors::NAVY),
	makeBGRA(colors::OLIVE),
	makeBGRA(colors::PURPE),
	makeBGRA(colors::TEAL),
};

// --------------------------------------------------------------------- //
// ----- macros -------------------------------------------------------- //
// --------------------------------------------------------------------- //

// --------------------------------------------------------------------- //
// ----- type definitions ---------------------------------------------- //
// --------------------------------------------------------------------- //

class MandelbrotWindow;

typedef int (*MandelBrotFunc)( int *cache, double xc, double yc, int max_iterations );

struct ProcessorData
{
	MandelbrotWindow	*window;
	Size				winSize;
	MandelBrotFunc		mandelBrotFunc;
	int					lowIter;
	double				lowMidValue;
	int					midIter;
	double				midHighValue;

	Scale				lowScale;
	Scale				midScale;
	Scale				highScale;

	ProcessorData(MandelbrotWindow*win) : window(win)
	{
	}
};

// --------------------------------------------------------------------- //
// ----- class definitions --------------------------------------------- //
// --------------------------------------------------------------------- //

class MandelbrotWindow : public OverlappedWindow
{
	friend gak::ProcessorType<int>;

	Range			m_xRange,
					m_yRange;
	Scale			m_xScale,
					m_yScale;

	gak::Buffer<int>	m_iterations;

	DirectBitmap	m_mandelBrot;

	Point			m_startSelect, 
					m_lastSelect;

	int				m_maxIterations;
	bool			m_showLabels;
	bool			m_doGray;
	int				m_centerColor;
	enum ColorMode 
	{
		cmLinear, cmRoot, cmLog, cmUnused
	}				m_colorMode;
	int				m_paletteIndex;
	void updateRanges( const Range &xRange, const Range &yRange );
	void updateScales();
	double getGrayBase( int iteratios ) const;
	BGRA findColor( int index, uint8 gray );
	void recalcLine( int line, ProcessorData *pd );
	void recalcBitmap( bool force, MandelBrotFunc mandelBrotFunc );

	ProcessStatus handleMouseMove( ModifierFlags modifier, const Point &position ) override;
	ProcessStatus handleLeftButton( LeftButton leftButton, ModifierFlags modifier, const Point &position ) override;
	ProcessStatus handleRightButton( RightButton rightButton, ModifierFlags modifier, const Point &position ) override;
	ProcessStatus handleResize( const Size &newSize ) override;
	ProcessStatus handleExitSizeMove() override;
	ProcessStatus handleRepaint( Device &hDC ) override;
	ProcessStatus handleCharacterInput( int c ) override;
	ProcessStatus handleKeyUp( int key ) override;
	ProcessStatus handleCommand( int cmd ) override;

	void handleTimer() override;

	public:
	MandelbrotWindow();
};

class WindowsApplication : public Application
{
	bool startApplication( HINSTANCE /*hInstance*/, const char * /*cmdLine*/ ) override
	{
		doEnableLogEx(gakLogging::llInfo);
		doDisableLog();
		setApplication("Mandelbrot");
		setCompany("gak");
		return 0;
	}
	CallbackWindow *createMainWindow( const char * /*cmdLine*/, int /*nCmdShow*/ ) override
	{
		std::unique_ptr<MandelbrotWindow>	mainWindow( new MandelbrotWindow );

		static ACCEL accelerators[] =
		{
			{ FVIRTKEY|FCONTROL,	'N', FILE_NEW_id },
			{ FVIRTKEY|FCONTROL,	'O', FILE_OPEN_id },
			{ FVIRTKEY|FCONTROL,	'S', FILE_SAVE_id },
			{ FVIRTKEY|FALT,		'X', FILE_EXIT_id },
		};
		mainWindow->setAccelerators( accelerators );

		if( mainWindow->create( nullptr ) == scERROR )
		{
			throw gak::LibraryException( "Could not create window!" );
		}

		mainWindow->focus();
		return mainWindow.release();
	}
	void deleteMainWindow( BasicWindow  *mainWindow ) override
	{
		delete mainWindow;
	}

	public:
	WindowsApplication() : Application( IDI_MANDELBROT ) {}
};

namespace gak
{
template <>
class ProcessorType<int>
{
	public:
	typedef int	object_type;

	static void process( int objectToProcess, void *, void *mainData  )
	{
		ProcessorData	*pd = (ProcessorData*)mainData;
		pd->window->recalcLine( objectToProcess, pd );
	}
};
}

// --------------------------------------------------------------------- //
// ----- exported datas ------------------------------------------------ //
// --------------------------------------------------------------------- //

// --------------------------------------------------------------------- //
// ----- module static data -------------------------------------------- //
// --------------------------------------------------------------------- //

static WindowsApplication	app;

// --------------------------------------------------------------------- //
// ----- class static data --------------------------------------------- //
// --------------------------------------------------------------------- //

// --------------------------------------------------------------------- //
// ----- prototypes ---------------------------------------------------- //
// --------------------------------------------------------------------- //

// --------------------------------------------------------------------- //
// ----- module functions ---------------------------------------------- //
// --------------------------------------------------------------------- //

static int calculateMandelbrotPixel2(double xc, double yc, int max_iterations)
{
	double x = 0.0;
	double y = 0.0;
	double x2 = 0.0;
	double y2 = 0.0;
    
	int iter = 0;
	while (x2 + y2 <= 4.0 && iter < max_iterations)
	{
		y = 2.0 * x * y + yc;
		x = x2 - y2 + xc;
        
		x2 = x * x;
		y2 = y * y;
		iter++;
	}
    
	return iter; // Rückgabe der Iterationszahl zur Farbwertberechnung
}

static int calculateMandelbrotPixel(int *cache, double xc, double yc, int max_iterations)
{
	return *cache = calculateMandelbrotPixel2(xc, yc, max_iterations);
}

static int getMandelbrotPixel(int *cache, double, double, int)
{
	return *cache;
}

// --------------------------------------------------------------------- //
// ----- class inlines ------------------------------------------------- //
// --------------------------------------------------------------------- //

// --------------------------------------------------------------------- //
// ----- class constructors/destructors -------------------------------- //
// --------------------------------------------------------------------- //

MandelbrotWindow::MandelbrotWindow() : OverlappedWindow( nullptr ), 
	m_xRange(X_RANGE), m_yRange(Y_RANGE), 
	m_xScale(Range(0,1),Range(0,1)), m_yScale(Range(0,1),Range(0,1)), 
	m_maxIterations(START_ITERATIONS), m_showLabels(true), m_colorMode(START_COLOR_MODE), m_doGray(false), m_centerColor(0),
	m_paletteIndex(START_PALETTE)
{
	setText("Apfelmänchen mit Mandelbrot");
}

// --------------------------------------------------------------------- //
// ----- class static functions ---------------------------------------- //
// --------------------------------------------------------------------- //

// --------------------------------------------------------------------- //
// ----- class privates ------------------------------------------------ //
// --------------------------------------------------------------------- //

void MandelbrotWindow::updateScales()
{
	Size winSize = getClientSize();
	m_xScale = Scale( Range( 0, winSize.width ), m_xRange );
	m_yScale = Scale( Range( winSize.height, 0 ), m_yRange );
}

void MandelbrotWindow::updateRanges( const Range &xRange, const Range &yRange )
{
	if( xRange.val1 < xRange.val2 && yRange.val1 < yRange.val2 )
	{
		m_xRange.val1 = gak::math::max(X_RANGE.val1, xRange.val1); 
		m_xRange.val2 = gak::math::min(X_RANGE.val2, xRange.val2); 
		m_yRange.val1 = gak::math::max(Y_RANGE.val1, yRange.val1); 
		m_yRange.val2 = gak::math::min(Y_RANGE.val2, yRange.val2); 

		updateScales();
		recalcBitmap(true, calculateMandelbrotPixel);
		invalidateWindow();
	}
}

double MandelbrotWindow::getGrayBase( int iter ) const
{
	const double t = (double)iter / m_maxIterations;

	const double colorT = m_colorMode==cmLinear 
		? t 
		: (m_colorMode==cmRoot 
			? sqrt(sqrt(t))
			:  (log(1.0 + 9.0 * t) / log(10.0))
		)
	;

	const double grayBase = 255.0 * colorT; 

	return grayBase;
}

BGRA MandelbrotWindow::findColor( int index, gak::uint8 gray )
{
	int channelFlags = COLORS[m_paletteIndex][index];
	gak::uint8 red = (channelFlags & RED) ? gray : 0;
	gak::uint8 green = (channelFlags & GREEN) ? gray : 0;
	gak::uint8 blue = (channelFlags & BLUE) ? gray : 0;

	return BGRA( blue, green, red );
}

void MandelbrotWindow::recalcLine( int y, ProcessorData *pd )
{
	int *cache = m_iterations + y * pd->winSize.width;
	const int maxY = gak::math::min( y+LINE_COUNT, pd->winSize.height );
	for( ; y<maxY; ++y )
	{
		const double yc = m_yScale( y );
		for( int x=0; x<pd->winSize.width; ++x )
		{
			const double xc = m_xScale( x );
			const int iter = pd->mandelBrotFunc( cache++, xc, yc, m_maxIterations );
			const double grayBase = getGrayBase( iter );
			if (iter == m_maxIterations) 
			{
				m_mandelBrot.setPixel( x, y, m_doGray ? BGRA(0, 0, 0) : CENTER_COLORS[m_centerColor] );
			}
			else 
			{
				if( m_doGray )
				{
					gak::uint8 gray = gak::math::round<gak::uint8>(grayBase);
					m_mandelBrot.setPixel( x, y, BGRA(gray, gray, gray) );
				}
				else if( iter <= pd->lowIter )
				{
					const gak::uint8 gray = gak::math::round<gak::uint8>(pd->lowScale(grayBase));
					m_mandelBrot.setPixel( x, y, findColor(0, gray) );
				}
				else if( iter <= pd->midIter )
				{
					const gak::uint8 gray = gak::math::round<gak::uint8>(pd->midScale(grayBase));
					m_mandelBrot.setPixel( x, y, findColor(1, gray) );
				}
				else
				{
					const gak::uint8 gray = gak::math::round<gak::uint8>(pd->highScale(grayBase));
					m_mandelBrot.setPixel( x, y, findColor(2, gray) );
				}
			}
		}
	}
}

void MandelbrotWindow::recalcBitmap(bool force, MandelBrotFunc mandelBrotFunc )
{
	static const double		maxColorVal = 255;

	gak::StopWatch			sw( true );

	const Size winSize = getClientSize();
	const Size &bitmapSize = m_mandelBrot.getSize();
	if( bitmapSize != winSize )
	{
		m_iterations.resize(winSize.width*winSize.height, true );
		m_mandelBrot.createBitmap( winSize );
		force = true;
	}
	if( force )
	{
		ProcessorData			data(this);

		gak::ThreadPool<int>	thePool( gak::Thread::getNumberOfCores(), "Mandelbrot", &data );

		data.mandelBrotFunc = mandelBrotFunc;
		data.lowIter = gak::math::round<int>(m_maxIterations/3.0);
		data.lowMidValue = getGrayBase(data.lowIter);
		data.midIter = gak::math::round<int>((m_maxIterations<<1)/3.0);
		data.midHighValue = getGrayBase(data.midIter);
		data.lowScale = Scale( Range(0, data.lowMidValue), Range(0, maxColorVal) );
		data.midScale = Scale( Range(data.lowMidValue, data.midHighValue), Range(0, maxColorVal) );
		data.highScale = Scale( Range(data.midHighValue, maxColorVal), Range(0, maxColorVal) );
		data.winSize = winSize;

		thePool.start();
		for( int y =0; y<winSize.height; y+=LINE_COUNT	)
		{
			thePool.process( y );

		}
		thePool.flush();
		thePool.shutdown();
		sw.stop();

		if( m_showLabels )
		{
			DrawDevice		myDC( this );
			MemoryDevice	memDC( myDC, m_mandelBrot );

			long ms = sw.getMillis();
			memDC.setMonospacedFont();
			memDC.textOut( 5, 5, gak::NumberBuffer().addNumber( ms ).addCP( "ms Hello world" ).c_str() );
			memDC.textOut( 5, 20, gak::NumberBuffer().addNumber( m_maxIterations ).c_str() );
			memDC.textOut( 5, 35, m_colorMode==cmLinear ? "Linear" : (m_colorMode==cmRoot ? "Square Root" : "Log") ); 
			memDC.textOut( 5, 50, 
				m_doGray 
					? "Graustufen"
					: gak::NumberBuffer().addNumber( m_paletteIndex ).c_str() 
			); 
			memDC.textOut( 5, 65, 
				gak::NumberBuffer().addNumber( m_centerColor ).c_str() 
			); 

			memDC.getFont().setCourier().setDirection( 900 ).create();
			memDC.setTextAlignment( Device::haCenter, Device::vaTop );
			memDC.textOut( 0, data.winSize.height/2, gak::NumberBuffer().addFloat( m_xRange.val1,0,6 ).c_str() );

			memDC.getFont().setDirection( 2700 );
			memDC.textOut( data.winSize.width, data.winSize.height/2, gak::NumberBuffer().addFloat( m_xRange.val2,0,6 ).c_str() );

			memDC.getFont().setDirection( 0 );
			memDC.textOut( data.winSize.width/2, 0, gak::NumberBuffer().addFloat( m_yRange.val2,0,6 ).c_str() );

			memDC.setTextAlignment( Device::haCenter, Device::vaBottom );
			memDC.textOut( data.winSize.width/2, data.winSize.height, gak::NumberBuffer().addFloat( m_yRange.val1,0,6 ).c_str() );
		}
	}
}

// --------------------------------------------------------------------- //
// ----- class protected ----------------------------------------------- //
// --------------------------------------------------------------------- //

// --------------------------------------------------------------------- //
// ----- class virtuals ------------------------------------------------ //
// --------------------------------------------------------------------- //

ProcessStatus MandelbrotWindow::handleMouseMove( ModifierFlags modifier, const Point &position )
{
	if( modifier.test( ModifierFlags::mbLeftMouseButton ) )
	{
		DrawDevice	dc( this );
		dc.setROP2( R2_NOT );
		if( m_lastSelect.x >= 0 )
		{
			dc.rectangle(m_startSelect.x, m_startSelect.y, m_lastSelect.x, m_lastSelect.y );
		}
		m_lastSelect = position;
		dc.rectangle(m_startSelect.x, m_startSelect.y, m_lastSelect.x, m_lastSelect.y );
	}
	return psPROCESSED;	
}

ProcessStatus MandelbrotWindow::handleLeftButton( LeftButton leftButton, ModifierFlags modifier, const Point &position )
{
	if( modifier.test(ModifierFlags::mbControlKey) )
	{
		updateRanges(X_RANGE, Y_RANGE);							// go to start
	}
	else if( leftButton == lbDOWN )
	{
		m_startSelect = position;								// begin selection
		m_lastSelect.x = -1;
		captureMouse();
	}
	else if( leftButton == lbUP )
	{															// end selection and zoom in
		releaseMouse();
		if( m_lastSelect.x >= 0 )
		{
			DrawDevice	dc( this );
			dc.setROP2( R2_NOT );
			dc.rectangle(m_startSelect.x, m_startSelect.y, m_lastSelect.x, m_lastSelect.y );
		}
		int minX = gak::math::min( m_startSelect.x, m_lastSelect.x );
		int maxX = gak::math::max( m_startSelect.x, m_lastSelect.x );

		int minY = gak::math::min( m_startSelect.y, m_lastSelect.y );
		int maxY = gak::math::max( m_startSelect.y, m_lastSelect.y );

		updateRanges(
			Range(m_xScale( minX ), m_xScale( maxX )),
			Range(m_yScale( maxY ), m_yScale( minY ))
		);
		m_lastSelect.x = -1;
	}

	return psPROCESSED;
}

ProcessStatus MandelbrotWindow::handleRightButton( RightButton rightButton, ModifierFlags modifier, const Point & )
{
	if( modifier.test(ModifierFlags::mbControlKey) )
	{
		updateRanges(X_RANGE, Y_RANGE);									// go to start
	}
	else if( rightButton == rbUP )
	{																	// zoom out
		const double xWidth = m_xRange.val2 - m_xRange.val1;
		const double yHeight = m_yRange.val2 - m_yRange.val1;

		const double xIncrement = xWidth/2;
		const double yIncrement = yHeight/2;

		updateRanges(
			Range(m_xRange.val1-xIncrement, m_xRange.val2+xIncrement),
			Range(m_yRange.val1-yIncrement,	m_yRange.val2+yIncrement)
		);
	}

	return psPROCESSED;
}

ProcessStatus MandelbrotWindow::handleResize( const Size &newSize )
{
	DisplayMode	dm = BasicWindow::getDisplayMode(&newSize);
	updateScales();
	if( dm == dmMAXIMIZED )
		handleTimer();			// recalc immediately
	else if( dm != dmMINIMIZED )
		setTimer( 500 );		// delayed recalc
	return OverlappedWindow::handleResize( newSize );
}

ProcessStatus MandelbrotWindow::handleExitSizeMove()
{
	handleTimer();			// recalc immediately
	return OverlappedWindow::handleExitSizeMove();
}

ProcessStatus MandelbrotWindow::handleRepaint( Device &hDC )
{
	hDC.drawBitmap( 0, 0, m_mandelBrot );
	return OverlappedWindow::handleRepaint( hDC );
}

ProcessStatus MandelbrotWindow::handleCharacterInput( int c )
{
	MandelBrotFunc	func = getMandelbrotPixel;

	if( c == 'b' )
		m_centerColor = nextIndex(CENTER_COLORS, m_centerColor);
	else if( c == 'B' )
		m_centerColor = prevIndex(CENTER_COLORS, m_centerColor);
	else if( c == 'e' )
		m_colorMode = ColorMode((int(m_colorMode)+1)%cmUnused);
	else if( c == 'g' )
		m_doGray = !m_doGray;
	else if( c == 'l' )
		m_showLabels = !m_showLabels;
	else if( c == 'p' )
		m_paletteIndex = nextIndex(COLORS, m_paletteIndex);
	else if( c == 'P' )
		m_paletteIndex = prevIndex(COLORS, m_paletteIndex);
	else if( c == 'q' )
		close();
	else if( c == 'r' )
	{
		func = calculateMandelbrotPixel;
		m_xRange = X_RANGE;
		m_yRange = Y_RANGE;
		updateScales();
	}
	else if( c == 'R' )
	{
		func = calculateMandelbrotPixel;
		m_xRange = X_RANGE;
		m_yRange = Y_RANGE;
		m_paletteIndex = START_PALETTE;
		m_maxIterations = START_ITERATIONS;
		m_colorMode = START_COLOR_MODE;
		m_centerColor = 0;
		m_doGray = false,
		updateScales();
	}
	else
	{
		return psPROCESSED;
	}

	recalcBitmap(true, func);
	invalidateWindow();
	return psPROCESSED;
}

ProcessStatus MandelbrotWindow::handleKeyUp( int key )
{
	const int divisor = isControlKey() ? 2 : 8;
	const double xOffset = (m_xRange.val2 - m_xRange.val1)/divisor;
	const double yOffset = (m_yRange.val2 - m_yRange.val1)/divisor;
	Range	newXrange = m_xRange;
	Range	newYrange = m_yRange;

	if( key == VK_UP )
	{
		if( !isShiftKey() )
			newYrange.val1 += yOffset;
		if( !isAltKey() )
			newYrange.val2 += yOffset;
	}
	else if( key == VK_DOWN )
	{
		if( !isShiftKey() )
			newYrange.val1 -= yOffset;
		if( !isAltKey() )
			newYrange.val2 -= yOffset;
	}
	else if( key == VK_LEFT )
	{
		if( !isShiftKey() )
			newXrange.val1 -= xOffset;
		if( !isAltKey() )
			newXrange.val2 -= xOffset;
	}
	else if( key == VK_RIGHT )
	{
		if( !isShiftKey() )
			newXrange.val1 += xOffset;
		if( !isAltKey() )
			newXrange.val2 += xOffset;
	}
	else if( key == VK_ADD || key == VK_OEM_PLUS )
	{	// allthough the ranges did not change, we need a rebuild and not the cache
		if( isControlKey() )
			m_maxIterations <<= 1;
		else
			m_maxIterations += isShiftKey() ? 1 : ITERATION_STEP;
	}
	else if( key == VK_SUBTRACT || key == VK_OEM_MINUS  )
	{	// allthough the ranges did not change, we need a rebuild and not the cache
		if( isControlKey() )
			m_maxIterations >>= 1;
		else
			m_maxIterations -= isShiftKey() ? 1 : ITERATION_STEP;

		if( m_maxIterations < MIN_ITERATIONS )
			m_maxIterations = MIN_ITERATIONS;
	}
	else
	{
		return OverlappedWindow::handleKeyUp( key );
	}
	updateRanges( newXrange, newYrange );
	return psPROCESSED;
}

ProcessStatus MandelbrotWindow::handleCommand( int cmd )
{
	switch( cmd )
	{
	case FILE_NEW_id:
		m_xRange = X_RANGE;
		m_yRange = Y_RANGE;
		m_paletteIndex = START_PALETTE;
		m_maxIterations = START_ITERATIONS;
		m_colorMode = START_COLOR_MODE;
		m_centerColor = 0;
		m_doGray = false,
		updateScales();
		recalcBitmap(true, calculateMandelbrotPixel);
		invalidateWindow();
		break;
			
	case FILE_OPEN_id:
	{
		OpenFileDialog	dialog;
		if( dialog.create( this, FILE_OPEN_TITLE, APFEL_FILES ) )
		{
			const char			*fileName = dialog.getFilename();
			gak::xml::Parser	theGuiParser( fileName );
			std::unique_ptr<gak::xml::Document>	newDoc( theGuiParser.readFile( false ) );
			if(newDoc)
			{
				gak::xml::Element *root = newDoc->getRoot();
				if( root )
				{
					gak::xml::Element *area = root->findElement("area");
					if( area )
					{
						double xMin = area->getAttribute("xMin").getValueE<double>();
						double xMax = area->getAttribute("xMax").getValueE<double>();
						double yMin = area->getAttribute("yMin").getValueE<double>();
						double yMax = area->getAttribute("yMax").getValueE<double>();

						m_maxIterations = area->getAttribute("maxInterations").getValueE<int>();
						m_paletteIndex = area->getAttribute("paletteIndex" ).getValueE<int>();
						m_colorMode = ColorMode(area->getAttribute("colorMode" ).getValueE<int>());

						m_doGray = area->getAttribute("doGray").getValueE<bool>();
						m_centerColor = area->getAttribute("centerColor").getValueE<int>();

						updateRanges(Range(xMin,xMax), Range(yMin,yMax));
					}
				}
			}
		}
		break;
	}
	case FILE_SAVE_id:
	{
		SaveFileAsDialog	dialog;
		if( dialog.create( this, FILE_SAVE_TITLE, APFEL_FILES ) )
		{
			const char *fileName = dialog.getFilename();
			std::unique_ptr<gak::xml::Document>	doc( new gak::xml::Document(fileName) );
			doc->addObject( new gak::xml::Declaration );
			gak::xml::Element *root = doc->addObject( new gak::xml::Any( "mandelbrot" ) );
			gak::xml::Element *area = root->addObject( new gak::xml::Any( "area" ) );

			area->setFloatAttribute("xMin", m_xRange.val1, -1);
			area->setFloatAttribute("xMax", m_xRange.val2, -1);
			area->setFloatAttribute("yMin", m_yRange.val1, -1);
			area->setFloatAttribute("yMax", m_yRange.val2, -1);
			area->setIntegerAttribute("maxInterations", m_maxIterations);
			area->setIntegerAttribute("paletteIndex", m_paletteIndex);
			area->setIntegerAttribute("colorMode", m_colorMode);
			area->setBoolAttribute("doGray", m_doGray);
			area->setBoolAttribute("centerColor", m_centerColor);

			STRING	xmlCode = doc->generate( gak::xml::XML_MODE );
			std::ofstream	stream( fileName );
			stream << xmlCode;
		}
		break;
	}
	case FILE_EXIT_id:
		close();
		break;
	default:
		return OverlappedWindow::handleCommand( cmd );
	}
	return psPROCESSED;
}

void MandelbrotWindow::handleTimer()
{
	recalcBitmap(false, calculateMandelbrotPixel);
	invalidateWindow();
	removeTimer();
}

// --------------------------------------------------------------------- //
// ----- class publics ------------------------------------------------- //
// --------------------------------------------------------------------- //

// --------------------------------------------------------------------- //
// ----- entry points -------------------------------------------------- //
// --------------------------------------------------------------------- //

#ifdef __BORLANDC__
#	pragma option -RT.
#	pragma option -b.
#	pragma option -a.
#	pragma option -p.
#endif

