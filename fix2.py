import io
p = r'C:\Users\Kille\MonkeyCode\cairn-src\src\mainwindow.cpp'
lines = io.open(p, 'r', encoding='utf-8', newline='').read().replace('\r\n', '\n').split('\n')

start = next(i for i, l in enumerate(lines) if l.strip() == '// ---- header')
end = next(i for i in range(start, len(lines)) if 'kind filter' in lines[i])

new = [
"    // ---- header",
"    // The title owns the top row outright. The controls used to sit on it too,",
"    // which left a long dead gap between the title and the right edge -- the",
"    // page read as a table with two empty columns. They now share the filter",
"    // row, where the eye already is when it starts narrowing the view.",
"    auto *headGrid = new QGridLayout(page);",
"    headGrid->setContentsMargins(0, 0, 0, 0);",
"    headGrid->setHorizontalSpacing(10);",
"    headGrid->setVerticalSpacing(1);",
"",
"    auto *back = new IconTextButton(Icons::Id::Home,",
"                                    QStringLiteral(\"Sessions\"), page);",
"    back->setObjectName(QStringLiteral(\"backLink\"));",
"    back->setToolTip(QStringLiteral(\"Back to every snapshot (Esc)\"));",
"    back->setFixedHeight(kBar);",
"    headGrid->addWidget(back, 0, 0, Qt::AlignLeft | Qt::AlignVCenter);",
"",
"    m_detailTitle = new QLabel(page);",
"    m_detailTitle->setObjectName(QStringLiteral(\"pageTitle\"));",
"    headGrid->addWidget(m_detailTitle, 0, 1, Qt::AlignLeft | Qt::AlignVCenter);",
"",
"    m_detailMeta = new QLabel(page);",
"    m_detailMeta->setObjectName(QStringLiteral(\"pageSubtitle\"));",
"    headGrid->addWidget(m_detailMeta, 1, 1, Qt::AlignLeft | Qt::AlignVCenter);",
"",
"    root->addLayout(headGrid);",
"",
"    // ---- toolbar: what the snapshot holds on the left, how to look at it on",
"    // the right. The chips sit in their own widget so rebuildKindFilter can",
"    // keep replacing its contents without ever touching these controls.",
"    auto *toolbar = new QHBoxLayout;",
"    toolbar->setContentsMargins(0, 10, 0, 0);",
"    toolbar->setSpacing(8);",
"",
]
lines[start:end] = new
t = '\n'.join(lines)

# Move the controls out of the header and onto the toolbar. They are built
# further down; append them to `toolbar` instead of `headGrid`.
moved = [
 ("    headGrid->addWidget(search, 0, 2, Qt::AlignRight | Qt::AlignVCenter);\n    m_find = search;",
  "    search->setFixedHeight(kBar);\n    toolbar->addWidget(search, 0, Qt::AlignVCenter);\n    m_find = search;"),
 ("    headGrid->addWidget(m_viewToggle, 0, 3, Qt::AlignRight | Qt::AlignVCenter);",
  "    m_viewToggle->setFixedHeight(kBar);\n    toolbar->addWidget(m_viewToggle, 0, Qt::AlignVCenter);"),
 ("    headGrid->addWidget(exportButton, 0, 4, Qt::AlignRight | Qt::AlignVCenter);",
  "    exportButton->setFixedHeight(kBar);\n    toolbar->addWidget(exportButton, 0, Qt::AlignVCenter);"),
]
for old, newv in moved:
    assert old in t, old[:60]
    t = t.replace(old, newv, 1)

# kBar must exist before the header uses it.
t = t.replace('''    root->setSpacing(0);

    // ---- header''', '''    root->setSpacing(0);

    // One height for every control in the header and toolbar. Mixed 24/27
    // heights leave the centres agreeing while the edges disagree, which still
    // reads as crooked.
    const int kBar = 26;

    // ---- header''', 1)
t = t.replace('''    // One height for every control in the row. Mixed 24/27 heights leave the
    // centres agreeing while the edges disagree, which still reads as crooked.
    const int kBar = 26;
''', '')

# The chips join the toolbar instead of the page, so they share its line.
t = t.replace('''    m_kindRow->setSpacing(6);
    root->addWidget(m_kindBar);''', '''    m_kindRow->setSpacing(6);
    toolbar->insertWidget(0, m_kindBar, 1);
    root->addLayout(toolbar);''', 1)

io.open(p, 'w', encoding='utf-8', newline='').write(t.replace('\n', '\r\n'))
print("toolbar row built")
