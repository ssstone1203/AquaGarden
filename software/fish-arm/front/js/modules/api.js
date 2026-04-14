/**
 * API service layer - handles all backend communication.
 * Separates concerns from UI logic in main.js etc.
 */
(function() {
    const API_BASE = window.AquaGarden?.CONFIG?.API_BASE_URL || 'http://localhost:8090';
    
    async function apiRequest(endpoint, options = {}) {
        const url = `${API_BASE}${endpoint.startsWith('/') ? '' : '/'}${endpoint}`;
        const headers = window.AquaGarden.getAuthHeaders();
        
        const config = {
            headers: { ...headers, ...options.headers },
            ...options
        };
        
        try {
            const response = await fetch(url, config);
            if (!response.ok) {
                if (response.status === 401) {
                    console.warn('Auth expired, redirecting to login');
                    localStorage.removeItem('token');
                    window.location.href = '/login.html';
                    return null;
                }
                throw new Error(`API Error: ${response.status}`);
            }
            return await response.json();
        } catch (error) {
            console.error('API request failed:', error);
            throw error;
        }
    }
    
    // Public API
    window.AquaGarden = window.AquaGarden || {};
    window.AquaGarden.api = {
        get: (endpoint) => apiRequest(endpoint, { method: 'GET' }),
        post: (endpoint, data) => apiRequest(endpoint, { 
            method: 'POST', 
            body: JSON.stringify(data) 
        }),
        updateSensors: () => window.AquaGarden.api.get('/api/v1/sensors'),
        controlRobot: (direction) => window.AquaGarden.api.post('/api/v1/robot/control', { direction }),
        switchMode: (mode) => window.AquaGarden.api.post('/api/v1/robot/mode', { mode })
    };
    
    console.log('✅ API service layer initialized (layered architecture)');
})();
