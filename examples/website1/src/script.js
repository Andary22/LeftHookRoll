function resetErrors() {
  document.getElementById("hours-error").textContent = "";
  document.getElementById("start-date-input-validation").textContent = "";
}

function submitSignUpForm(event) {
  event.preventDefault();

  const emailInput = document.getElementById("email-input");
  const nameInput = document.getElementById("name-input");
  const phoneInput = document.getElementById("phone-input");
  const passwordInput = document.getElementById("password-input");
  const confirmPasswordInput = document.getElementById("password-conf-input");
  const educationSelect = document.getElementById("education");

  let isValid = true;

  if (!emailInput.value || !/^\S+@\S+\.\S+$/.test(emailInput.value)) {
    showError(emailInput, "Please enter a valid email address.");
    isValid = false;
  } else {
    clearError(emailInput);
  }

  if (!nameInput.value || nameInput.value.trim().length < 2) {
    showError(nameInput, "Name must be at least 2 characters long.");
    isValid = false;
  } else {
    clearError(nameInput);
  }

  if (!phoneInput.value || !/^\d{12}$/.test(phoneInput.value)) {
    showError(phoneInput, "Please enter a valid phone number (12 digits).");
    isValid = false;
  } else {
    clearError(phoneInput);
  }

  if (!passwordInput.value || passwordInput.value.length < 8) {
    showError(passwordInput, "Password must be at least 8 characters long.");
    isValid = false;
  } else {
    clearError(passwordInput);
  }

  if (passwordInput.value !== confirmPasswordInput.value) {
    showError(confirmPasswordInput, "Passwords do not match.");
    isValid = false;
  } else {
    clearError(confirmPasswordInput);
  }

  if (!educationSelect.value) {
    showError(educationSelect, "Please select your education level.");
    isValid = false;
  } else {
    clearError(educationSelect);
  }

  if (isValid) {
    alert("Form submitted successfully!");
  }
}

function showError(inputElement, message) {
  const errorElement = inputElement.nextElementSibling;
  errorElement.textContent = message;
  inputElement.classList.add("is-invalid");
}

function clearError(inputElement) {
  const errorElement = inputElement.nextElementSibling;
  errorElement.textContent = "";
  inputElement.classList.remove("is-invalid");
}

document
  .getElementById("signupForm")
  .addEventListener("submit", submitSignUpForm);
