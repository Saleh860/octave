////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2021-2024 The Octave Project Developers
//
// See the file COPYRIGHT.md in the top-level directory of this
// distribution or <https://octave.org/copyright/>.
//
// This file is part of Octave.
//
// Octave is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Octave is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Octave; see the file COPYING.  If not, see
// <https://www.gnu.org/licenses/>.
//
////////////////////////////////////////////////////////////////////////

#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#if defined (HAVE_QSCINTILLA)

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QTextBlock>
#include <QVBoxLayout>

#include "command-widget.h"

#include "cmd-edit.h"
#include "event-manager.h"
#include "gui-preferences-cs.h"
#include "gui-preferences-global.h"
#include "gui-settings.h"
#include "gui-utils.h"
#include "input.h"
#include "interpreter.h"

OCTAVE_BEGIN_NAMESPACE(octave)

command_widget::command_widget (QWidget *p)
  : QWidget (p), m_incomplete_parse (false),
    m_prompt (QString ()),
    m_console (new console (this))
{
  QPushButton *pause_button = new QPushButton (tr("Pause"), this);
  QPushButton *stop_button = new QPushButton (tr("Stop"), this);
  QPushButton *resume_button = new QPushButton (tr("Continue"), this);

  QGroupBox *input_group_box = new QGroupBox ();
  QHBoxLayout *input_layout = new QHBoxLayout;
  input_layout->addWidget (pause_button);
  input_layout->addWidget (stop_button);
  input_layout->addWidget (resume_button);
  input_group_box->setLayout (input_layout);

  QVBoxLayout *main_layout = new QVBoxLayout ();
  main_layout->addWidget (m_console);
  main_layout->addWidget (input_group_box);

  setLayout (main_layout);

  setFocusProxy (m_console);

  connect (pause_button, &QPushButton::clicked,
           this, &command_widget::interpreter_pause);

  connect (resume_button, &QPushButton::clicked,
           this, &command_widget::interpreter_resume);

  connect (stop_button, &QPushButton::clicked,
           this, &command_widget::interpreter_stop);

  connect (this, &command_widget::update_prompt_signal,
           this, &command_widget::update_prompt);

  connect (this, &command_widget::new_command_line_signal,
           m_console, &console::new_command_line);

  connect (m_console, qOverload<const fcn_callback&> (&console::interpreter_event),
           this, qOverload<const fcn_callback&> (&command_widget::interpreter_event));

  connect (m_console, qOverload<const meth_callback&> (&console::interpreter_event),
           this, qOverload<const meth_callback&> (&command_widget::interpreter_event));

  insert_interpreter_output ("\n\n    Welcome to Octave\n\n");

}

void
command_widget::init_command_prompt ()
{
  // The interpreter_event callback function below emits a signal.
  // Because we don't control when that happens, use a guarded pointer
  // so that the callback can abort if this object is no longer valid.

  QPointer<command_widget> this_cw (this);

  emit interpreter_event
    ([this, this_cw] (interpreter& interp)
     {
       // INTERPRETER THREAD

       // We can skip the entire callback function because it does not
       // make any changes to the interpreter state.

       if (this_cw.isNull ())
         return;

       std::string prompt = interp.PS1 ();
       std::string decoded_prompt
         = command_editor::decode_prompt_string (prompt);

       emit update_prompt_signal (QString::fromStdString (decoded_prompt));

       emit new_command_line_signal ();
     });
}

void
command_widget::update_prompt (const QString& prompt)
{
  m_prompt = prompt;
}

QString
command_widget::prompt ()
{
  return m_prompt;
}

void
command_widget::insert_interpreter_output (const QString& msg)
{
  m_console->append (msg);
}

void
command_widget::process_input_line (const QString& input_line)
{
  // The interpreter_event callback function below emits a signal.
  // Because we don't control when that happens, use a guarded pointer
  // so that the callback can abort if this object is no longer valid.

  QPointer<command_widget> this_cw (this);

  emit interpreter_event
    ([this, this_cw, input_line] (interpreter& interp)
     {
       // INTERPRETER THREAD

       // If THIS_CW is no longer valid, we still want to parse and
       // execute INPUT_LINE but we can't emit the signals associated
       // with THIS_CW.

       interp.parse_and_execute (input_line.toStdString (),
                                 m_incomplete_parse);

       if (this_cw.isNull ())
         return;

       std::string prompt
         = m_incomplete_parse ? interp.PS2 () : interp.PS1 ();

       std::string decoded_prompt
         = command_editor::decode_prompt_string (prompt);

       emit update_prompt_signal (QString::fromStdString (decoded_prompt));

       emit new_command_line_signal ();
     });

}

void
command_widget::notice_settings ()
{
  gui_settings settings;

  // Set terminal font:
  QFont term_font = QFont ();
  term_font.setStyleHint (QFont::TypeWriter);
  QString default_font = settings.string_value (global_mono_font);
  term_font.setFamily
    (settings.value (cs_font.settings_key (), default_font).toString ());
  term_font.setPointSize
    (settings.int_value (cs_font_size));

  m_console->setFont (term_font);

  // Colors
  int mode = settings.int_value (cs_color_mode);
  QColor fgc = settings.color_value (cs_colors[0], mode);
  QColor bgc = settings.color_value (cs_colors[1], mode);

  m_console->setStyleSheet (QString ("color: %1; background-color:%2;")
                            .arg (fgc.name ()).arg (bgc.name ()));
}

// The console itself using QScintilla.
// This implementation is partly based on the basic concept of
// "qpconsole" as proposed by user "DerManu" in the Qt-forum thread
// https://forum.qt.io/topic/28765/command-terminal-using-qtextedit

console::console (command_widget *p)
  : QsciScintilla (p),
    m_command_position (-1),
    m_cursor_position (0),
    m_text_changed (false),
    m_command_widget (p),
    m_last_key_string (QString ())
{
  setMargins (0);
  setWrapMode (QsciScintilla::WrapWord);

  connect (this, SIGNAL (cursorPositionChanged (int, int)),
           this, SLOT (cursor_position_changed (int, int)));

  connect (this, SIGNAL (textChanged ()),
           this, SLOT (text_changed ()));

  connect (this, SIGNAL (modificationAttempted ()),
           this, SLOT (move_cursor_to_end ()));
}

// Prepare a new command line with the current prompt
void
console::new_command_line (const QString& command)
{
  if (! text (lines () -1).isEmpty ())
    append ("\n");

  append_string (m_command_widget->prompt ());

  int line, index;
  getCursorPosition (&line, &index);
  m_command_position = positionFromLineIndex (line, index);

  append_string (command);
}

// Accept the current command line (or block)
void
console::accept_command_line ()
{
  QString input_line = text (lines () - 1);

  if (input_line.startsWith (m_command_widget->prompt ()))
    input_line.remove (0, m_command_widget->prompt ().length ());

  input_line = input_line.trimmed ();

  append_string ("\n");

  if (input_line.isEmpty ())
    new_command_line ();
  else
    m_command_widget->process_input_line (input_line);
}

// Execute a command
void
console::execute_command (const QString& command)
{
  if (command.trimmed ().isEmpty ())
    return;

  new_command_line (command);
  accept_command_line ();
}

// Append a string and update the curdor püosition
void
console::append_string (const QString& string)
{
  setReadOnly (false);
  append (string);

  int line, index;
  lineIndexFromPosition (text ().length (), &line, &index);

  setCursorPosition (line, index);
}

// Cursor position changed: Are we in the command line or not?
void
console::cursor_position_changed (int line, int col)
{
  m_cursor_position = positionFromLineIndex (line, col);
  if (m_cursor_position < m_command_position)
    {
      // We are in the read only area
      if (m_text_changed && (m_cursor_position == m_command_position - 1))
        {
          setReadOnly (false);
          insert (m_command_widget->prompt ().right (1)); // And here we have tried to remove the prompt by Backspace
          setCursorPosition (line+1, col);
        }
      setReadOnly (true);
    }
  else
    setReadOnly (false);  // Writable area

  m_text_changed = false;
}

// User attempted to type on read only mode: move cursor at end and allow
// editing
void
console::move_cursor_to_end ()
{
  if ((! m_last_key_string.isEmpty ()) && (m_last_key_string.at (0).isPrint ()))
    {
      append_string (m_last_key_string);
      setReadOnly (true); // Avoid that changing read only text is done afterwards
    }
}

// Text has changed: is cursor still in "writable" area?
// This signal seems to be emitted before cursor position changed.
void
console::text_changed ()
{
  m_text_changed = true;
}

// Re-implement key event
void
console::keyPressEvent (QKeyEvent *e)
{
  if (e->key () == Qt::Key_Return)
    // On "return", accept the current command line
    accept_command_line ();
  else
    {
      // Otherwise, store text process the expected event
      m_last_key_string = e->text ();
      QsciScintilla::keyPressEvent (e);
    }
}

OCTAVE_END_NAMESPACE(octave)

#endif
