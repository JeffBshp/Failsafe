#include <string.h>
#include <ctype.h>
#include "SDL2/SDL.h"
#include "shape.h"
#include "utility.h"
#include "editor.h"

static int GetCharCol(char* s, int nCols, int i)
{
	if (i <= 0) return 0;

	for (int j = 1; i - j >= 0; j++)
	{
		if (s[i - j] == '\n') return (j - 1) % nCols;
	}

	return i % nCols;
}

static inline int GetLineStart(int i, int col)
{
	i -= col;
	if (i < 0) i = 0;
	return i;
}

static int GetColInLine(char* s, int start, int col)
{
	if (start < 0) start = 0;
	if (col < 0) col = 0;

	for (int i = start; i < start + col; i++)
	{
		if (s[i] == '\n' || s[i] == '\0')
			return i;
	}

	return start + col;
}

static int GetNextLineStart(char* s, int nCols, int i, int col)
{
	if (i < 0) i = 0;
	if (col < 0) col = 0;
	int end = i + nCols - col;

	while (i < end && s[i] != '\0')
		if (s[i++] == '\n') break;

	return i;
}

static void MoveCursor(TextBox* tb, int newI, bool modShift)
{
	if (!modShift || newI == tb->i)
		tb->selectStart = -1;
	else if (modShift && tb->selectStart < 0)
		tb->selectStart = tb->i;

	tb->i = newI;
}

static void Delete(TextBox* tb)
{
	char* s = tb->text;
	int start = tb->selectStart;
	int i = tb->i;

	if (start < 0 || start == i)
	{
		start = i++;
	}
	else if (start > i)
	{
		int temp = start;
		start = i;
		i = temp;
	}

	int gap = i - start;
	tb->i = start;
	tb->selectStart = -1;

	for (; s[start + gap] != '\0'; start++)
	{
		s[start] = s[start + gap];
	}

	s[start] = '\0';
}

static int SkipWord(char* s, int i, bool right)
{
	int dir = right ? 1 : -1;
	char c = s[i + dir];
	bool wasAlpha = isalpha(c);

	while (i + dir >= 0 && c != '\0' && ((bool)isalpha(c)) == wasAlpha)
	{
		i += dir;
		c = s[i + dir];
	}

	if ((!right && !wasAlpha) || (right && wasAlpha))
	{
		while (i + dir >= 0 && c != '\0' && ((bool)isalpha(c)) != wasAlpha)
		{
			i += dir;
			c = s[i + dir];
		}
	}

	if (right && c != '\0') i++;

	return i;
}

void Editor_Edit(TextBox* tb, SDL_Keysym sym)
{
	SDL_KeyCode c = sym.sym;
	char* s = tb->text;
	int nSlots = tb->nCols * tb->nRows;
	int i = tb->i;
	int n = strlen(s);
	bool modShift = (sym.mod & KMOD_SHIFT) != 0;
	bool modCtrl = (sym.mod & KMOD_CTRL) != 0;

	if (i > 0 && c == SDLK_BACKSPACE)
	{
		if (tb->selectStart < 0) tb->i--;
		Delete(tb);
	}
	else if (i < n && c == SDLK_DELETE)
	{
		Delete(tb);
	}
	else if (i > 0 && c == SDLK_LEFT)
	{
		if (modCtrl) i = SkipWord(s, i, false);
		else i--;
		MoveCursor(tb, i, modShift);
	}
	else if (i < n && c == SDLK_RIGHT)
	{
		if (modCtrl) i = SkipWord(s, i, true);
		else i++;
		MoveCursor(tb, i, modShift);
	}
	else if (i > 0 && c == SDLK_UP)
	{
		int col = GetCharCol(s, tb->nCols, i);
		int start = GetLineStart(i, col);
		int prevCol = GetCharCol(s, tb->nCols, start - 1);
		int prevStart = GetLineStart(start - 1, prevCol);
		int newI = GetColInLine(s, prevStart, col);
		MoveCursor(tb, newI, modShift);
	}
	else if (i < n && c == SDLK_DOWN)
	{
		int col = GetCharCol(s, tb->nCols, i);
		int start = GetLineStart(i, col);
		int nextStart = GetNextLineStart(s, tb->nCols, i, col);
		if (s[nextStart] == '\0' && GetCharCol(s, tb->nCols, nextStart) > 0) nextStart = start;
		int newI = tb->i = GetColInLine(s, nextStart, col);
		MoveCursor(tb, newI, modShift);
	}
	else if (i > 0 && c == SDLK_HOME)
	{
		int col = GetCharCol(s, tb->nCols, i);
		int home = GetLineStart(i, col);
		MoveCursor(tb, home, modShift);
	}
	else if (i < n && c == SDLK_END)
	{
		int col = GetCharCol(s, tb->nCols, i);
		int end = GetNextLineStart(s, tb->nCols, i, col);
		if (s[end] != '\0') end--;
		MoveCursor(tb, end, modShift);
	}
	else if (n < nSlots)
	{
		if ((c >= ' ' && c <= '~') || c == SDLK_TAB || c == SDLK_RETURN)
		{
			for (; n >= i; n--)
				s[n + 1] = s[n];

			if (c == SDLK_TAB)
			{
				c = '\t';
			}
			else if (c == SDLK_RETURN)
			{
				c = '\n';
			}
			else if (modShift)
			{
				if (c >= SDLK_a && c <= SDLK_z)
					c -= 32;
				else switch (c)
				{
				case '`': c = '~'; break;
				case '1': c = '!'; break;
				case '2': c = '@'; break;
				case '3': c = '#'; break;
				case '4': c = '$'; break;
				case '5': c = '%'; break;
				case '6': c = '^'; break;
				case '7': c = '&'; break;
				case '8': c = '*'; break;
				case '9': c = '('; break;
				case '0': c = ')'; break;
				case '-': c = '_'; break;
				case '=': c = '+'; break;

				case '[': c = '{'; break;
				case ']': c = '}'; break;
				case '\\': c = '|'; break;
				case ';': c = ':'; break;
				case '\'': c = '"'; break;
				case ',': c = '<'; break;
				case '.': c = '>'; break;
				case '/': c = '?'; break;
				}
			}

			s[i] = c;
			tb->i++;
		}
	}
}

void Editor_Update(TextBox* tb, int ticks)
{
	int nChars = strlen(tb->text);
	int nSlots = tb->nCols * tb->nRows;
	bool showCursor = tb->focused && (ticks / 100) % 8 != 0;
	bool tab = false;
	bool newLine = false;
	int i = 0;

	for (int slot = 0; slot < nSlots; slot++)
	{
		char c = i < nChars ? tb->text[i] : '\0';
		int tex = TEX_BLANK; // invisible character
		bool consume = true; // should this slot consume a char from the string

		if (tab)
		{
			consume = false;
			if ((slot % tb->nCols) % 4 == 3) tab = false;
		}
		else if (newLine)
		{
			consume = false;
			if ((slot + 1) % tb->nCols == 0) newLine = false;
		}
		else if (c == ' ')
		{
			if (tb->showWhiteSpace) tex = TEX_SPACE;
		}
		else if (c > ' ' && c <= '~')
		{
			tex = c - ' ';
		}
		else if (c == '\t')
		{
			if (tb->showWhiteSpace) tex = TEX_TAB;
			if ((slot % tb->nCols) % 4 < 3) tab = true;
		}
		else if (c == '\n')
		{
			if (tb->showWhiteSpace) tex = TEX_RETURN;
			if ((slot + 1) % tb->nCols != 0) newLine = true;
		}

		if (consume && showCursor)
		{
			if (tb->selectStart >= 0 && tb->selectStart < tb->i)
			{
				if (i >= tb->selectStart && i < tb->i) tex += TEX_SET1;
			}
			else if (tb->selectStart >= 0 && tb->selectStart > tb->i)
			{
				if (i >= tb->i && i < tb->selectStart) tex += TEX_SET1;
			}
			else if (i == tb->i) tex += TEX_SET1;
		}

		tb->shape->instanceData[slot * 17] = tex + tb->texOffset;
		if (consume) i++;
	}
}

void Editor_SaveToFile(TextBox* tb, char* filePath)
{
	FILE* file = fopen(filePath, "w");
	int i = 0, n = tb->nCols * tb->nRows;
	while (i < n && tb->text[i] != '\0')
		fwrite(tb->text + i++, sizeof(char), 1, file);
	fclose(file);
}
