export default function attribute(name, value, condition) {
    if (typeof condition === 'undefined') {
        condition = true;
    }
    return condition ? { [name]: value } : null;
};
