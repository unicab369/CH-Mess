const columns = [
  '#', 'Part No.', 'Freq', 'Flash', 'SRAM', 'VDD MIN', 'VDD MAX', 'GPIO',
  'SPI', 'IIC', 'UART', 'RTC', 'IIS', 'CAN', 'USB2.0', 'Features', 'Package', 'Price'
];

const head = document.querySelector('#table-head');
const body = document.querySelector('#table-body');
const search = document.querySelector('#search');
const packageFilter = document.querySelector('#package-filter');
const resultCount = document.querySelector('#result-count');
const tableFrame = document.querySelector('.table-frame');
const table = document.querySelector('table');
const columnPicker = document.querySelector('#column-picker');
const columnMenu = document.querySelector('#column-menu');

let chips = [];
let sortColumn = 0;
let sortDirection = 1;
const defaultVisibleColumns = new Set([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 16, 17]);
const visibleColumns = new Set(defaultVisibleColumns);
const columnWidths = [38, 82, 62, 52, 52, 62, 62, 56, 54, 54, 54, 54, 54, 54, 96, 72, 100, 78];

function parseCSV(text) {
  const rows = [];
  let row = [];
  let field = '';
  let quoted = false;

  for (let i = 0; i < text.length; i += 1) {
    const char = text[i];
    if (quoted) {
      if (char === '"' && text[i + 1] === '"') { field += '"'; i += 1; }
      else if (char === '"') quoted = false;
      else field += char;
    } else if (char === '"') quoted = true;
    else if (char === ',') { row.push(field.trim()); field = ''; }
    else if (char === '\n' || char === '\r') {
      if (char === '\r' && text[i + 1] === '\n') i += 1;
      row.push(field.trim()); field = '';
      if (row.some((cell) => cell !== '')) rows.push(row);
      row = [];
    } else field += char;
  }
  if (field.length || row.length) { row.push(field.trim()); if (row.some((cell) => cell !== '')) rows.push(row); }
  return rows;
}

function renderHeader() {
  head.innerHTML = '';
  columns.forEach((column, index) => {
    const th = document.createElement('th');
    th.scope = 'col';
    th.dataset.column = index;
    const button = document.createElement('button');
    button.type = 'button';
    button.innerHTML = `${column}<span class="sort-mark" aria-hidden="true">↕</span>`;
    button.addEventListener('click', () => {
      if (sortColumn === index) sortDirection *= -1;
      else { sortColumn = index; sortDirection = 1; }
      render();
    });
    th.append(button);
    head.append(th);
  });
}

function buildColumnPicker() {
  columnMenu.replaceChildren();
  const selectAllLabel = document.createElement('label');
  const selectAll = document.createElement('input');
  selectAll.type = 'checkbox';
  selectAll.checked = visibleColumns.size === columns.length;
  selectAllLabel.append(selectAll, document.createTextNode('Select all'));
  columnMenu.append(selectAllLabel);
  const defaultButton = document.createElement('button');
  defaultButton.type = 'button';
  defaultButton.className = 'menu-action';
  defaultButton.textContent = 'Default';
  defaultButton.addEventListener('click', () => {
    visibleColumns.clear();
    defaultVisibleColumns.forEach((index) => visibleColumns.add(index));
    columnMenu.querySelectorAll('input[data-column]').forEach((checkbox) => {
      checkbox.checked = visibleColumns.has(Number(checkbox.dataset.column));
    });
    updateSelectAll();
    applyColumnVisibility();
  });
  columnMenu.append(defaultButton);
  const divider = document.createElement('div');
  divider.className = 'menu-divider';
  columnMenu.append(divider);

  columns.forEach((column, index) => {
    if (index < 2) return;
    const label = document.createElement('label');
    const checkbox = document.createElement('input');
    checkbox.type = 'checkbox';
    checkbox.dataset.column = index;
    checkbox.checked = visibleColumns.has(index);
    label.append(checkbox, document.createTextNode(column));
    columnMenu.append(label);
  });

  selectAll.addEventListener('change', () => {
    visibleColumns.clear();
    (selectAll.checked ? columns.map((_, index) => index) : [...defaultVisibleColumns])
      .forEach((index) => visibleColumns.add(index));
    columnMenu.querySelectorAll('input[data-column]').forEach((checkbox) => {
      checkbox.checked = visibleColumns.has(Number(checkbox.dataset.column));
    });
    updateSelectAll();
    applyColumnVisibility();
  });

  columnMenu.querySelectorAll('input[data-column]').forEach((checkbox) => {
    checkbox.addEventListener('change', () => {
      const index = Number(checkbox.dataset.column);
      if (checkbox.checked) visibleColumns.add(index);
      else visibleColumns.delete(index);
      updateSelectAll();
      applyColumnVisibility();
    });
  });

  function updateSelectAll() {
    const allSelected = columns.slice(2).every((_, offset) => visibleColumns.has(offset + 2));
    const noneSelected = columns.slice(2).every((_, offset) => !visibleColumns.has(offset + 2));
    selectAll.checked = allSelected;
    selectAll.indeterminate = !allSelected && !noneSelected;
  }
}

function applyColumnVisibility() {
  const visibleWidth = columns.reduce((total, _, index) => total + (visibleColumns.has(index) ? columnWidths[index] : 0), 0);
  table.style.width = `${visibleWidth}px`;
  document.querySelectorAll('thead th').forEach((cell, index) => {
    cell.hidden = !visibleColumns.has(index);
  });
  body.querySelectorAll('tr').forEach((row) => {
    row.querySelectorAll('td').forEach((cell, index) => {
      cell.hidden = !visibleColumns.has(index);
    });
  });
}

function render() {
  const query = search.value.trim().toLowerCase();
  const selectedPackage = packageFilter.value;
  const filtered = chips.filter((chip) => {
    const matchesText = chip.some((value) => value.toLowerCase().includes(query));
    const matchesPackage = !selectedPackage || chip[16] === selectedPackage;
    return matchesText && matchesPackage;
  });

  filtered.sort((a, b) => {
    const left = a[sortColumn] || '';
    const right = b[sortColumn] || '';
    const leftNumber = Number.parseFloat(left.replace(/[^\d.]/g, ''));
    const rightNumber = Number.parseFloat(right.replace(/[^\d.]/g, ''));
    const comparison = Number.isNaN(leftNumber) || Number.isNaN(rightNumber)
      ? left.localeCompare(right, undefined, { numeric: true, sensitivity: 'base' })
      : leftNumber - rightNumber;
    return comparison * sortDirection;
  });

  body.replaceChildren();
  if (!filtered.length) {
    const row = document.createElement('tr');
    const cell = document.createElement('td');
    cell.className = 'message';
    cell.colSpan = columns.length;
    cell.textContent = 'No chips match that search.';
    row.append(cell);
    body.append(row);
  } else {
    const fragment = document.createDocumentFragment();
    filtered.forEach((chip) => {
      const row = document.createElement('tr');
      chip.forEach((value, index) => {
        const cell = document.createElement('td');
        cell.textContent = value;
        if (index === 0) cell.className = 'chip-id';
        row.append(cell);
      });
      fragment.append(row);
    });
    body.append(fragment);
  }

  resultCount.textContent = `${filtered.length} of ${chips.length} chips`;
  head.querySelectorAll('th').forEach((th, index) => {
    const active = index === sortColumn;
    th.setAttribute('aria-sort', active ? (sortDirection === 1 ? 'ascending' : 'descending') : 'none');
    th.querySelector('.sort-mark').textContent = active ? (sortDirection === 1 ? '↑' : '↓') : '↕';
  });
  applyColumnVisibility();
}

function showError(message) {
  resultCount.textContent = 'Could not load chip data';
  body.innerHTML = '';
  const row = document.createElement('tr');
  const cell = document.createElement('td');
  cell.className = 'message';
  cell.colSpan = columns.length;
  cell.textContent = message;
  row.append(cell);
  body.append(row);
}

async function loadChips() {
  try {
    let csvText;
    if (location.protocol === 'file:') {
      // Browsers block fetch() of sibling files from file:// pages, so the CSV
      // is embedded in index.html as a standalone-file fallback.
      csvText = document.querySelector('#embedded-chip-csv')?.textContent;
      if (!csvText) throw new Error('Embedded chip data is missing.');
    } else {
      const response = await fetch('./chips.csv');
      if (!response.ok) throw new Error(`CSV request failed (${response.status}).`);
      csvText = await response.text();
    }
    const records = parseCSV(csvText);
    const header = records.shift();
    if (!header || header.length !== columns.length) throw new Error('The CSV header should contain 17 columns.');
    chips = records.filter((record) => record.length === columns.length);
    if (!chips.length) throw new Error('The CSV does not contain any chip rows.');

    [...new Set(chips.map((chip) => chip[16]).filter(Boolean))].sort().forEach((item) => {
      const option = document.createElement('option');
      option.value = item;
      option.textContent = item.replaceAll('_', ' ');
      packageFilter.append(option);
    });
    renderHeader();
    buildColumnPicker();
    render();
  } catch (error) {
    showError(`${error.message} Run a local web server to load chips.csv.`);
  }
}

search.addEventListener('input', render);
packageFilter.addEventListener('change', render);
document.addEventListener('keydown', (event) => {
  if (event.key === '/' && !['INPUT', 'TEXTAREA', 'SELECT'].includes(document.activeElement.tagName)) {
    event.preventDefault();
    search.focus();
  }
  if (event.key === 'Escape' && document.activeElement === search) {
    search.value = '';
    render();
    search.blur();
  }
  if (event.key === 'Escape') columnPicker.open = false;
});
document.addEventListener('click', (event) => {
  if (columnPicker.open && !columnPicker.contains(event.target)) columnPicker.open = false;
});

loadChips();
