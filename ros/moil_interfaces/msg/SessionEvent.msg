# A session changed. Published for every mutation.
#
# This is what makes a second client correct rather than merely possible: a
# client that edits a cell is not the only one whose view has to move. Every
# connected client hears this and re-fetches with SessionState, which answers
# "unchanged" for the one that caused it and sends the table to the others.

std_msgs/Header header

string session_id
uint64 version

# "created" | "opened" | "closed" | "renamed" | "saved" | "removed" | "edited"
# | "captured" | "computed" | "undone" | "redone"
string kind

# Free text for the log line: which slot, which op, how many cells.
string detail
