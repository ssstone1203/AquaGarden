/**
 * Configuration and constants for AquaGarden frontend - LAYERED MODULE.
 * Centralizes API config, auth helpers, theme consistency across all pages.
 * Used by main.js, login.js, etc. to ensure uniform styling and API calls.
 */
const CONFIG = {
    API_BASE_URL: 'http://localhost:8090',
    WS_URL: 'ws://localhost:8090/api/ws/logs',
    DEFAULT_TOKEN_KEY: 'token',
    REFRESH_INTERVAL: 5000, // ms for sensor updates
    THEME: {
        primary: '#7c3aed',
        primaryLight: '#8b5cf6',
        success: '#10b981',
        warning: '#f59e0b',
        danger: '#ef4444',
        info: '#3b82f6',
    }
};

// Common auth headers helper
function getAuthHeaders(token = null) {
    const authToken = token || localStorage.getItem(CONFIG.DEFAULT_TOKEN_KEY);
    return {
        'Content-Type': 'application/json',
        ...(authToken && { 'Authorization': `Bearer ${authToken}` })
    };
}

// Apply consistent theme and styles across ALL pages (login, index, robot, etc.)
function applyConsistentTheme() {
    if (document.getElementById('aquagarden-theme')) return; // prevent duplicates
    
    const style = document.createElement('style');
    style.id = 'aquagarden-theme';
    style.textContent = `
        :root {
            --primary-color: #7c3aed;
            --primary-light: #8b5cf6;
            --success-color: #10b981;
            --warning-color: #f59e0b;
            --danger-color: #ef4444;
            --info-color: #3b82f6;
        }
        
        .btn-primary, button.primary, .nav-link.active, .sidebar a.active {
            background-color: var(--primary-color) !important;
            border-color: var(--primary-color) !important;
            color: white !important;
        }
        
        .card, .panel, .content-card {
            border-left: 4px solid var(--primary-color);
        }
        
        .sensor-temp { color: var(--danger-color); font-weight: bold; }
        .sensor-ph { color: var(--info-color); font-weight: bold; }
        .sensor-oxygen { color: var(--success-color); font-weight: bold; }
        .sensor-turbidity { color: var(--warning-color); font-weight: bold; }
        
        .status-success { color: var(--success-color); }
        .status-warning { color: var(--warning-color); }
    `;
    document.head.appendChild(style);
    console.log('✅ AquaGarden consistent theme applied (layered CSS)');
}

// Make available globally for other scripts
window.AquaGarden = window.AquaGarden || {};
window.AquaGarden.CONFIG = CONFIG;
window.AquaGarden.getAuthHeaders = getAuthHeaders;
window.AquaGarden.applyConsistentTheme = applyConsistentTheme;

// Auto apply theme when loaded
if (typeof document !== 'undefined') {
    document.addEventListener('DOMContentLoaded', applyConsistentTheme);
}
