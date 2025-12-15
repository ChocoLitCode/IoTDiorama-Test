// ===== LocalStorage Functions (for all local data) =====
function saveToLocalStorage(key, data) {
    try {
        localStorage.setItem(key, JSON.stringify(data));
    } catch (e) {
        console.warn('localStorage save failed:', e);
    }
}

function loadFromLocalStorage(key, defaultValue) {
    try {
        const saved = localStorage.getItem(key);
        return saved ? JSON.parse(saved) : defaultValue;
    } catch (e) {
        console.warn('localStorage load failed:', e);
        return defaultValue;
    }
}

// Dark mode toggle with persistence
document.getElementById("darkToggle").addEventListener("click", () => {
    document.body.classList.toggle("dark-mode");
    const isDark = document.body.classList.contains("dark-mode");
    saveToLocalStorage('darkMode', isDark);
    updateChartTheme();
});

// Notification panel toggle
const notifPanel = document.getElementById("notifPanel");
document.getElementById("notifToggle").addEventListener("click", () => {
    notifPanel.classList.add("open");
});
document.getElementById("notifClose").addEventListener("click", () => {
    notifPanel.classList.remove("open");
});

// GRAPH PANEL TOGGLE
const graphPanel = document.getElementById("graphPanel");
document.getElementById("graphToggle").addEventListener("click", () => {
    graphPanel.classList.add("open");
    // Initialize charts when panel opens
    if (!chartsInitialized) {
        if (typeof Chart === 'undefined') {
            console.error('Chart.js not loaded! Check if chart.umd.min.js is being served.');
            return;
        }
        initCharts();
        setupFilterListeners();
        chartsInitialized = true;
    }
});
document.getElementById("graphClose")?.addEventListener("click", () => {
    graphPanel.classList.remove("open");
});


// ===== WebSocket Variables =====
let websocket = null;
let reconnectDelay = 2000;
const maxReconnectDelay = 5000;

// DOM Elements
const temperatureElement = document.getElementById("temperature");
const humidityElement = document.getElementById("humidity");
const tempTimeElement = document.getElementById("tempTime");
const humidTimeElement = document.getElementById("humidTime");
const soundBadgeElement = document.getElementById("soundBadge");
const soundTimeElement = document.getElementById("soundTime");
const doorStateElement = document.getElementById("doorState");

// Room 1 elements
const room1OnBtn = document.getElementById("room1On");
const room1OffBtn = document.getElementById("room1Off");
const room1AutoBtn = document.getElementById("room1Auto");
const room1StateTxt = document.getElementById("room1State");
const light1State = document.getElementById("light1State");

// Room 2 elements
const room2OnBtn = document.getElementById("room2On");
const room2OffBtn = document.getElementById("room2Off");
const room2AutoBtn = document.getElementById("room2Auto");
const room2StateTxt = document.getElementById("room2State");
const light2State = document.getElementById("light2State");

// ===== Notification System =====
let notifications = loadFromLocalStorage('notifications', []);
let pendingNotifications = [];
const MAX_NOTIFICATIONS = 20;

function addNotification(type, message) {
    const timestamp = new Date().toLocaleString();
    const notification = {
        type: type,
        message: message,
        timestamp: timestamp,
        id: Date.now()
    };
    
    // If limit reached, add to pending queue
    if (notifications.length >= MAX_NOTIFICATIONS) {
        pendingNotifications.push(notification);
        if (pendingNotifications.length === 1) {
            alert('Notification limit reached! New notifications are queued. Clear notifications to see them.');
        }
        return;
    }
    
    notifications.unshift(notification); // Add to beginning
    
    displayNotifications();
    updateNotificationBadge();
    saveToLocalStorage('notifications', notifications);
}

function displayNotifications() {
    const notifContent = document.querySelector('.notif-content');
    
    if (notifications.length === 0) {
        notifContent.innerHTML = '<div class="notif-item">No new notifications.</div>';
        return;
    }
    
    let html = '';
    notifications.forEach(notif => {
        let icon = '';
        let className = 'notif-item';
        
        switch(notif.type) {
            case 'intruder':
                icon = '🚨';
                className += ' notif-danger';
                break;
            case 'failed':
                icon = '⚠️';
                className += ' notif-warning';
                break;
            default:
                icon = 'ℹ️';
        }
        
        html += `
            <div class="${className}">
                <div class="notif-icon">${icon}</div>
                <div class="notif-body">
                    <div class="notif-message">${notif.message}</div>
                    <div class="notif-time">${notif.timestamp}</div>
                </div>
            </div>
        `;
    });
    
    notifContent.innerHTML = html;
}

function updateNotificationBadge() {
    const badge = document.getElementById('notifBadge');
    if (!badge) {
        // Create badge if it doesn't exist
        const notifToggle = document.getElementById('notifToggle');
        const newBadge = document.createElement('div');
        newBadge.id = 'notifBadge';
        newBadge.className = 'notif-badge';
        
        // Wrap notifToggle in a container for positioning
        const wrapper = document.createElement('div');
        wrapper.style.position = 'relative';
        wrapper.style.display = 'inline-block';
        notifToggle.parentNode.insertBefore(wrapper, notifToggle);
        wrapper.appendChild(notifToggle);
        wrapper.appendChild(newBadge);
    }
    
    const badgeElement = document.getElementById('notifBadge');
    const count = notifications.length;
    
    if (count > 0) {
        badgeElement.textContent = count >= 21 ? '20+' : count;
        badgeElement.style.display = 'block';
    } else {
        badgeElement.style.display = 'none';
    }
}

function clearNotifications() {
    notifications = [];
    // Process pending notifications from queue
    while (pendingNotifications.length > 0 && notifications.length < MAX_NOTIFICATIONS) {
        const pending = pendingNotifications.shift();
        notifications.unshift(pending);
    }
    displayNotifications();
    updateNotificationBadge();
    saveToLocalStorage('notifications', notifications);
    if (pendingNotifications.length > 0) {
        alert(`${pendingNotifications.length} notification(s) still queued. Clear again to see more.`);
    }
}

// ===== Chart Data =====
let allChartData = loadFromLocalStorage('allChartData', []);

function saveChartDataLocal() {
    saveToLocalStorage('allChartData', allChartData);
}

function loadChartDataLocal() {
    allChartData = loadFromLocalStorage('allChartData', []);
}

// ===== Chart Variables =====
let chartsInitialized = false;
let combinedChart;
 
// Data storage with full timestamps (unlimited)
// let allChartData = []; // Removed duplicate declaration

// Filter state
let currentDateFilter = null; // Specific date string or null for all
let currentTimeFrom = null;
let currentTimeTo = null;
let viewOffset = 0; // For < > navigation (in slides)

// ===== Chart Configuration =====
function getChartColors() {
    const isDark = document.body.classList.contains('dark-mode');
    return {
        temp: {
            line: isDark ? '#ff6b9d' : '#e74c3c',
            fill: isDark ? 'rgba(255, 107, 157, 0.3)' : 'rgba(231, 76, 60, 0.3)',
            fillEnd: isDark ? 'rgba(255, 107, 157, 0.05)' : 'rgba(231, 76, 60, 0.05)'
        },
        humid: {
            line: isDark ? '#5eb3f6' : '#3498db',
            fill: isDark ? 'rgba(94, 179, 246, 0.3)' : 'rgba(52, 152, 219, 0.3)',
            fillEnd: isDark ? 'rgba(94, 179, 246, 0.05)' : 'rgba(52, 152, 219, 0.05)'
        },
        trend: {
            line: isDark ? '#7ed321' : '#27ae60',
            fill: isDark ? 'rgba(126, 211, 33, 0.3)' : 'rgba(39, 174, 96, 0.3)',
            fillEnd: isDark ? 'rgba(126, 211, 33, 0.05)' : 'rgba(39, 174, 96, 0.05)'
        },
        grid: isDark ? 'rgba(255, 255, 255, 0.1)' : 'rgba(0, 0, 0, 0.1)',
        text: isDark ? '#b8c2ff' : '#000000'
    };
}

function initCharts() {
    const colors = getChartColors();
    
    // Combined chart with dual Y-axes
    const ctx = document.getElementById('combinedChart').getContext('2d');
    combinedChart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: [],
            datasets: [{
                label: 'Temperature (°C)',
                data: [],
                borderColor: colors.temp.line,
                backgroundColor: colors.temp.fill,
                borderWidth: 2.5,
                fill: true,
                tension: 0.4,
                pointRadius: 3,
                pointHoverRadius: 5,
                pointBackgroundColor: colors.temp.line,
                pointBorderColor: '#fff',
                pointBorderWidth: 2,
                yAxisID: 'y'
            }, {
                label: 'Humidity (%)',
                data: [],
                borderColor: colors.humid.line,
                backgroundColor: colors.humid.fill,
                borderWidth: 2.5,
                fill: true,
                tension: 0.4,
                pointRadius: 3,
                pointHoverRadius: 5,
                pointBackgroundColor: colors.humid.line,
                pointBorderColor: '#fff',
                pointBorderWidth: 2,
                yAxisID: 'y1'
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            interaction: {
                mode: 'index',
                intersect: false,
            },
            plugins: {
                legend: {
                    display: true,
                    position: 'top',
                    labels: {
                        color: colors.text,
                        usePointStyle: true,
                        padding: 15
                    }
                },
                tooltip: {
                    enabled: true,
                    backgroundColor: colors.text === '#b8c2ff' ? 'rgba(26, 26, 46, 0.9)' : 'rgba(255, 255, 255, 0.9)',
                    titleColor: colors.text,
                    bodyColor: colors.text,
                    borderColor: colors.grid,
                    borderWidth: 1,
                    padding: 12
                }
            },
            scales: {
                x: {
                    grid: {
                        color: colors.grid,
                        drawBorder: false
                    },
                    ticks: {
                        color: colors.text,
                        maxRotation: 0,
                        minRotation: 0,
                        autoSkipPadding: 30,
                        maxTicksLimit: 8,
                        font: {
                            size: 11
                        }
                    }
                },
                y: {
                    type: 'linear',
                    position: 'left',
                    grid: {
                        color: colors.grid,
                        drawBorder: false
                    },
                    ticks: {
                        color: colors.text,
                        font: {
                            size: 11
                        },
                        callback: function(value) {
                            return value.toFixed(1) + '°C';
                        }
                    },
                    title: {
                        display: true,
                        text: 'Temperature (°C)',
                        color: colors.text
                    }
                },
                y1: {
                    type: 'linear',
                    position: 'right',
                    min: 0,
                    max: 100,
                    grid: {
                        drawOnChartArea: false
                    },
                    ticks: {
                        color: colors.text,
                        font: {
                            size: 11
                        },
                        callback: function(value) {
                            return value.toFixed(0) + '%';
                        }
                    },
                    title: {
                        display: true,
                        text: 'Humidity (%)',
                        color: colors.text
                    }
                }
            }
        }
    });

    console.log(`Chart initialized with ${allChartData.length} data points`);
    chartsInitialized = true;
    updateChartDisplay();
}

async function updateCharts(temp, humid) {
    if (!chartsInitialized) return;

    const now = new Date();
    
    // Store with full timestamp
    allChartData.push({
        timestamp: now.getTime(),
        dateStr: now.toLocaleDateString('en-US'),
        timeStr: now.toLocaleTimeString('en-US', { hour: '2-digit', minute: '2-digit', second: '2-digit', hour12: false }),
        temperature: temp,
        humidity: humid
    });

    // Auto-cleanup: Keep only last 7 days of data
    const sevenDaysAgo = now.getTime() - (7 * 24 * 60 * 60 * 1000);
    allChartData = allChartData.filter(d => d.timestamp >= sevenDaysAgo);

    // Update display with current filter
    updateChartDisplay();
}

function getFilteredData() {
    let filtered = [...allChartData];

    // Apply specific date filter
    if (currentDateFilter) {
        // Convert selected date to match format in data (format: M/D/YYYY or MM/DD/YYYY)
        const [year, month, day] = currentDateFilter.split('-');
        const selectedDate = new Date(parseInt(year), parseInt(month) - 1, parseInt(day));
        const dateStr = selectedDate.toLocaleDateString('en-US');
        filtered = filtered.filter(d => d.dateStr === dateStr);
    }
    // If null, show all dates

    // Apply time range filter
    if (currentTimeFrom || currentTimeTo) {
        filtered = filtered.filter(d => {
            const time = d.timeStr;
            const from = currentTimeFrom || '00:00:00';
            const to = currentTimeTo || '23:59:59';
            return time >= from && time <= to;
        });
    }

    // Navigation offset is now handled in updateChartDisplay() with slide-based pagination
    return filtered;
}

function updateChartDisplay() {
    if (!chartsInitialized) return;

    const filtered = getFilteredData();
    
    // Limit to 7 points per view based on viewOffset
    const pointsPerView = 7;
    
    // When filters are active (date or time), start from beginning
    // When no filters, show most recent data
    let startIndex, endIndex;
    if (currentDateFilter || currentTimeFrom || currentTimeTo) {
        // Show from start of filtered data, paginate forward
        startIndex = viewOffset * pointsPerView;
        endIndex = startIndex + pointsPerView;
    } else {
        // Show most recent data, paginate backward
        startIndex = Math.max(0, filtered.length - pointsPerView - (viewOffset * pointsPerView));
        endIndex = Math.max(pointsPerView, filtered.length - (viewOffset * pointsPerView));
    }
    
    const viewData = filtered.slice(startIndex, endIndex);
    
    // Format labels as "MM/DD HH:MM:SS AM/PM"
    const labels = viewData.map(d => {
        const date = new Date(d.timestamp);
        const month = String(date.getMonth() + 1).padStart(2, '0');
        const day = String(date.getDate()).padStart(2, '0');
        let hours = date.getHours();
        const minutes = String(date.getMinutes()).padStart(2, '0');
        const seconds = String(date.getSeconds()).padStart(2, '0');
        const ampm = hours >= 12 ? 'PM' : 'AM';
        hours = hours % 12 || 12;
        return `${month}/${day} ${hours}:${minutes}:${seconds} ${ampm}`;
    });
    
    combinedChart.data.labels = labels;
    combinedChart.data.datasets[0].data = viewData.map(d => d.temperature);
    combinedChart.data.datasets[1].data = viewData.map(d => d.humidity);
    combinedChart.update('none');
}

function setupFilterListeners() {
    // Date input filter
    const dateInput = document.getElementById('dateFilter');
    if (dateInput) {
        // Set default to today
        const today = new Date().toISOString().split('T')[0];
        dateInput.value = today;
        currentDateFilter = today;
        
        dateInput.addEventListener('change', () => {
            currentDateFilter = dateInput.value;
            viewOffset = 0; // Reset navigation
            updateChartDisplay();
        });
    }
    
    // Show all dates button
    document.getElementById('showAllDates')?.addEventListener('click', () => {
        currentDateFilter = null;
        if (dateInput) dateInput.value = '';
        viewOffset = 0;
        updateChartDisplay();
    });

    // Time range filter
    document.getElementById('applyTimeFilter').addEventListener('click', () => {
        const from = document.getElementById('timeFrom').value;
        const to = document.getElementById('timeTo').value;
        currentTimeFrom = from ? from + ':00' : null;
        currentTimeTo = to ? to + ':59' : null;
        viewOffset = 0; // Reset navigation
        updateChartDisplay();
    });

    // Reset filter
    document.getElementById('resetFilter').addEventListener('click', () => {
        currentTimeFrom = null;
        currentTimeTo = null;
        viewOffset = 0;
        document.getElementById('timeFrom').value = '00:00';
        document.getElementById('timeTo').value = '23:59';
        updateChartDisplay();
    });

    // Navigation buttons
    document.getElementById('navPrev').addEventListener('click', () => {
        // When filters active: prev = earlier data (decrease offset if positive)
        // When no filters: prev = older data (increase offset)
        if (currentDateFilter || currentTimeFrom || currentTimeTo) {
            viewOffset = Math.max(0, viewOffset - 1); // Go to earlier slide
        } else {
            viewOffset += 1; // Go to older data
        }
        updateChartDisplay();
    });

    document.getElementById('navNext').addEventListener('click', () => {
        // When filters active: next = later data (increase offset)
        // When no filters: next = newer data (decrease offset, min 0)
        if (currentDateFilter || currentTimeFrom || currentTimeTo) {
            viewOffset += 1; // Go to later slide
        } else {
            viewOffset = Math.max(0, viewOffset - 1); // Go to newer data
        }
        updateChartDisplay();
    });
}

function updateChartTheme() {
    if (!chartsInitialized || !combinedChart) return;
    
    const colors = getChartColors();
    
    // Update datasets
    combinedChart.data.datasets[0].borderColor = colors.temp.line;
    combinedChart.data.datasets[0].backgroundColor = colors.temp.fill;
    combinedChart.data.datasets[0].pointBackgroundColor = colors.temp.line;
    
    combinedChart.data.datasets[1].borderColor = colors.humid.line;
    combinedChart.data.datasets[1].backgroundColor = colors.humid.fill;
    combinedChart.data.datasets[1].pointBackgroundColor = colors.humid.line;
    
    // Update scales
    combinedChart.options.scales.x.grid.color = colors.grid;
    combinedChart.options.scales.x.ticks.color = colors.text;
    combinedChart.options.scales.y.grid.color = colors.grid;
    combinedChart.options.scales.y.ticks.color = colors.text;
    combinedChart.options.scales.y.title.color = colors.text;
    combinedChart.options.scales.y1.ticks.color = colors.text;
    combinedChart.options.scales.y1.title.color = colors.text;
    
    // Update legend
    combinedChart.options.plugins.legend.labels.color = colors.text;
    
    // Update tooltip
    combinedChart.options.plugins.tooltip.backgroundColor = colors.text === '#b8c2ff' ? 'rgba(26, 26, 46, 0.9)' : 'rgba(255, 255, 255, 0.9)';
    combinedChart.options.plugins.tooltip.titleColor = colors.text;
    combinedChart.options.plugins.tooltip.bodyColor = colors.text;
    combinedChart.options.plugins.tooltip.borderColor = colors.grid;
    
    combinedChart.update();
}

// ===== WebSocket Functions =====
function getCurrentTime() {
    return new Date().toLocaleTimeString();
}

function updateTimeDisplay(target) {
    if (target) target.textContent = getCurrentTime();
}

function setControlsEnabled(enabled) {
    [room1OnBtn, room1OffBtn, room1AutoBtn, room2OnBtn, room2OffBtn, room2AutoBtn].forEach(btn => {
        if (btn) btn.disabled = !enabled;
    });
}


function initWebSocket() {
    const gateway = `ws://${window.location.hostname}/ws`;
    try {
        websocket = new WebSocket(gateway);
    } catch (e) {
        console.error('WebSocket creation failed', e);
        scheduleReconnect();
        return;
    }

    websocket.onopen = () => {
        console.log('WebSocket connected');
        setControlsEnabled(true);
        websocket.send('getReadings');
        reconnectDelay = 2000;
    };

    websocket.onclose = () => {
        console.log('WebSocket disconnected');
        setControlsEnabled(false);
        scheduleReconnect();
    };

    websocket.onerror = (err) => {
        console.warn('WebSocket error', err);
    };

    websocket.onmessage = onMessage;
}

function scheduleReconnect() {
    if (websocket) {
        try { websocket.close(); } catch (e) {}
        websocket = null;
    }
    const delay = Math.min(reconnectDelay, maxReconnectDelay);
    console.log(`Reconnecting in ${delay} ms...`);
    setTimeout(() => {
        initWebSocket();
        reconnectDelay = Math.min(maxReconnectDelay, reconnectDelay * 2.5);
    }, delay);
}

function sendUnlockDoorCommand() {
    if (websocket && websocket.readyState === WebSocket.OPEN) {
        websocket.send("unlockDoor");
    }
}

function sendLockDoorCommand() {
    if (websocket && websocket.readyState === WebSocket.OPEN) {
        websocket.send("lockDoor");
    }
}

function onMessage(event) {
    try {
        const data = JSON.parse(event.data);

        // Temperature & Humidity
        const temp = parseFloat(data.temperature ?? data.temp);
        const humid = parseFloat(data.humidity ?? data.h);

        if (!isNaN(temp)) {
            temperatureElement.textContent = temp.toFixed(1);
            updateTimeDisplay(tempTimeElement);
        }
        if (!isNaN(humid)) {
            humidityElement.textContent = humid.toFixed(1);
            updateTimeDisplay(humidTimeElement);
        }

        // Update charts
        if (!isNaN(temp) && !isNaN(humid)) {
            updateCharts(temp, humid);
        }

        // Sound Detection
        if (data.sound !== undefined) {
            updateTimeDisplay(soundTimeElement);
            
            if (data.sound === "detected") {
                soundBadgeElement.textContent = "Activate";
                soundBadgeElement.className = "sound-badge detected";
            } else if (data.sound === "listening") {
                soundBadgeElement.textContent = "Listening";
                soundBadgeElement.className = "sound-badge listening";
            } else if (data.sound === "quiet") {
                soundBadgeElement.textContent = "Quiet";
                soundBadgeElement.className = "sound-badge quiet";
            }
        }

        // Alert notifications from door system
        if (data.alert !== undefined) {
            if (data.alert === "Access Denied") {
                addNotification('intruder', 'INTRUDER ALERT - Access Denied!');
                // Show pop-up alert for intruder
                showIntruderAlert();
            } else if (data.alert === "Failed Attempt") {
                addNotification('failed', 'Failed door access attempt detected');
            }
        }

        // Door status
        if (data.door !== undefined) {
            doorStateElement.textContent = data.door;
            doorStateElement.className = data.door === "UNLOCKED" ? "state-box active" : "state-box inactive";
        }

        // Room 1 State
        if (data.room1 !== undefined) {
            const state = String(data.room1);
            const mode = data.room1Mode || (state === "AUTO" ? "AUTO" : "MANUAL");
            
            // Update visual mode (border colors) using separate mode field
            setRoomMode(1, mode);
            
            // Update status box based on actual state
            if (state === "ON") {
                light1State.textContent = "Active";
                light1State.className = "state-box active";
            } else {
                light1State.textContent = "Inactive";
                light1State.className = "state-box inactive";
            }
        }

        // Room 2 State
        if (data.room2 !== undefined) {
            const state = String(data.room2);
            const mode = data.room2Mode || (state === "AUTO" ? "AUTO" : "MANUAL");
            
            // Update visual mode (border colors) using separate mode field
            setRoomMode(2, mode);
            
            // Update status box based on actual state
            if (state === "ON") {
                light2State.textContent = "Active";
                light2State.className = "state-box active";
            } else {
                light2State.textContent = "Inactive";
                light2State.className = "state-box inactive";
            }
        }

    } catch (err) {
        console.error("WS Parse Error:", err);
    }
}

// Track last temperature notification to prevent spam
let lastTempNotif = { level: '', time: 0 };
// Temperature alert feature removed

// Show intruder alert pop-up
function showIntruderAlert() {
    const alertDiv = document.createElement('div');
    alertDiv.className = 'intruder-alert-popup';
    alertDiv.innerHTML = `
        <div class="intruder-alert-content">
            <div class="intruder-alert-icon">🚨</div>
            <div class="intruder-alert-title">INTRUDER DETECTED!</div>
            <div class="intruder-alert-message">Unauthorized access attempt blocked</div>
            <button class="intruder-alert-btn" onclick="closeIntruderAlert()">OK</button>
        </div>
    `;
    document.body.appendChild(alertDiv);
    
    // Auto-close after 10 seconds
    setTimeout(() => {
        closeIntruderAlert();
    }, 10000);
}

function closeIntruderAlert() {
    const alert = document.querySelector('.intruder-alert-popup');
    if (alert) {
        alert.remove();
    }
}

// Track current mode for each room to prevent flickering
let room1CurrentMode = 'AUTO';
let room2CurrentMode = 'AUTO';

// Toggle visual mode (Auto/Manual) for a room
function setRoomMode(roomNumber, mode) {
    const btn = document.getElementById(`room${roomNumber}Auto`);
    const stateEl = document.getElementById(`room${roomNumber}State`);
    const card = btn ? btn.closest('.control-card') : null;
    if (!btn || !stateEl) return;

    const m = String(mode ?? '').toUpperCase();
    
    // Determine if this is AUTO or MANUAL mode
    const isAuto = (m === 'AUTO');
    const currentMode = roomNumber === 1 ? room1CurrentMode : room2CurrentMode;
    
    // Only update CSS classes if mode category changed (AUTO <-> MANUAL)
    if (isAuto && currentMode !== 'AUTO') {
        // Switching to AUTO mode
        btn.classList.add('mode-auto');
        btn.classList.remove('mode-manual');
        if (card) {
            card.classList.add('state-auto');
            card.classList.remove('state-manual');
        }
        if (roomNumber === 1) room1CurrentMode = 'AUTO';
        else room2CurrentMode = 'AUTO';
    } else if (!isAuto && currentMode === 'AUTO') {
        // Switching from AUTO to MANUAL mode
        btn.classList.remove('mode-auto');
        btn.classList.add('mode-manual');
        if (card) {
            card.classList.remove('state-auto');
            card.classList.add('state-manual');
        }
        if (roomNumber === 1) room1CurrentMode = 'MANUAL';
        else room2CurrentMode = 'MANUAL';
    }
    // If already in the correct mode category, don't touch CSS classes
    
    // Always update state text
    stateEl.textContent = isAuto ? 'Auto' : m;
}

// ===== Room Control Functions =====
function sendRoom1Command(cmd) {
    if (websocket && websocket.readyState === WebSocket.OPEN) {
        websocket.send("room1:" + cmd);
    } else {
        console.warn('WebSocket not ready - ignoring command');
    }
}

function sendRoom2Command(cmd) {
    if (websocket && websocket.readyState === WebSocket.OPEN) {
        websocket.send("room2:" + cmd);
    } else {
        console.warn('WebSocket not ready - ignoring command');
    }
}

// ===== Room Control Event Listeners =====
room1OnBtn.addEventListener("click", () => { sendRoom1Command("ON"); });
room1OffBtn.addEventListener("click", () => { sendRoom1Command("OFF"); });
room1AutoBtn.addEventListener("click", () => { sendRoom1Command("AUTO"); });

room2OnBtn.addEventListener("click", () => { sendRoom2Command("ON"); });
room2OffBtn.addEventListener("click", () => { sendRoom2Command("OFF"); });
room2AutoBtn.addEventListener("click", () => { sendRoom2Command("AUTO"); });

// Door control
const unlockDoorBtn = document.getElementById("unlockDoorBtn");
const lockDoorBtn = document.getElementById("lockDoorBtn");
unlockDoorBtn.addEventListener("click", sendUnlockDoorCommand);
lockDoorBtn.addEventListener("click", sendLockDoorCommand);

// Add clear notifications button
document.getElementById("clearNotif")?.addEventListener("click", clearNotifications);

// ===== Header Date/Time Update =====
function updateHeaderDateTime() {
    const now = new Date();
    const options = { weekday: 'long', year: 'numeric', month: 'long', day: 'numeric' };
    const dateStr = now.toLocaleDateString('en-US', options);
    const timeStr = now.toLocaleTimeString('en-US', { hour: '2-digit', minute: '2-digit', second: '2-digit' });
    
    const headerDateTime = document.getElementById('headerDateTime');
    if (headerDateTime) {
        headerDateTime.textContent = `${dateStr} • ${timeStr}`;
    }
}

// ===== Info Panel Toggle =====
function toggleInfoPanel() {
    const infoPanel = document.getElementById('infoPanel');
    if (infoPanel) {
        infoPanel.classList.toggle('open');
    }
}

// ===== Logout Function =====
function logout() {
    // Confirm before logging out
    if (confirm('Are you sure you want to logout?')) {
        // Clear authentication
        localStorage.removeItem('auth');
        // Optionally clear all data
        // localStorage.clear();
        // Redirect to login page
        window.location.href = 'login.html';
    }
}

// ===== Initialization =====
window.addEventListener("load", async () => {
    console.log('App loaded. Chart.js available:', typeof Chart !== 'undefined');
    
    // Load data from PouchDB
    notifications = await loadNotificationsPouchDB();
    allChartData = await loadChartDataPouchDB();
    
    // Restore dark mode preference
    const savedDarkMode = loadFromLocalStorage('darkMode', false);
    if (savedDarkMode) {
        document.body.classList.add('dark-mode');
    }
    
    // Start header clock
    updateHeaderDateTime();
    setInterval(updateHeaderDateTime, 1000); // Update every second
    
    // Display saved notifications
    displayNotifications();
    updateNotificationBadge();
    
    // Log PouchDB stats
    const chartPoints = allChartData.length;
    const notifCount = notifications.length;
    console.log(`Restored from PouchDB: ${chartPoints} chart points, ${notifCount} notifications`);
    
    // Initialize charts immediately
    if (typeof Chart !== 'undefined') {
        initCharts();
        setupFilterListeners();
        chartsInitialized = true;
    }
    
    setControlsEnabled(false);
    initWebSocket();
    
    // Apply initial visual state for room modes based on current DOM text
    try {
        setRoomMode(1, (room1StateTxt && room1StateTxt.textContent) || 'AUTO');
        setRoomMode(2, (room2StateTxt && room2StateTxt.textContent) || 'AUTO');
    } catch (e) { console.warn('Could not set initial room mode:', e); }
    
    // Logout button
    const logoutBtn = document.getElementById('logoutBtn');
    if (logoutBtn) {
        logoutBtn.addEventListener('click', logout);
    }
});