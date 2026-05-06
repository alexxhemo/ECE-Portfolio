import { useState } from 'react';
import { useNavigate, useLocation } from 'react-router-dom';
import { assignSleepRole, getRoleDescription } from '../utils/RoleAssignment';
import './Survey.css';

const Survey = () => {
  const [step, setStep] = useState(0);
  const [answers, setAnswers] = useState({});
  const [userRole, setUserRole] = useState(null);
  const [numberInput, setNumberInput] = useState('');
  const navigate = useNavigate();
  const location = useLocation();

   // API Configuration
  const API_URL = "https://hijlnlbnb2.execute-api.us-east-2.amazonaws.com/prod";
  const redirectTo = location.state?.redirectTo || '/dashboard';

  const submitSurveyToAPI = async (surveyData) => {
    try {
      const response = await fetch(`${API_URL}/survey`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          userId: 'user-' + Date.now(), // Temporary ID - replace with real auth later
          responses: surveyData
        }),
      });

      if (!response.ok) throw new Error('Failed to get results from server');
      return await response.json();
    } catch (error) {
      console.error('API Error:', error);
      throw error;
    }
  };

  // Get the redirect destination from signup, default to dashboard
  const redirectTo = location.state?.redirectTo || '/dashboard';

  const questions = [
    // Page 6 - Welcome
    {
      type: 'welcome',
      title: 'Welcome to Snooze Streak',
      description: "In the following section we will be asking a few questions to better determine the type of sleeper you are and help you set on the best sleep pattern.",
      buttonText: 'NEXT'
    },
    // Page 7 - Sleep Pattern 1
    {
      type: 'radio',
      title: 'Sleep Pattern',
      question: 'How long does it take you to sleep?',
      options: ['Less than 15 min', '15-30 min', 'More than 30 min'],
      key: 'sleepDuration',
      roleMapping: {
        'More than 30 min': 'Restless Relaxer'
      }
    },
    // Page 8 - Sleep Pattern 2
    {
      type: 'yesno',
      title: 'Sleep Pattern',
      question: 'Do you wake up during night and have trouble falling back asleep?',
      key: 'nightWakeups',
      roleMapping: {
        'Yes': 'Fragmented Sleeper'
      }
    },
    // Page 9 - Sleep Routine 1
    {
      type: 'radio',
      title: 'Sleep Routine',
      question: 'How many hours of sleep do you usually get per night?',
      options: ['Less than 7', '7-9', 'More than 9'],
      key: 'hoursOfSleep',
      roleMapping: {
        'Less than 7': 'Sleep-Deprived Dreamer',
        '7-9': 'Healthy Sleeper',
        'More than 9': 'Oversleeping Owl'
      }
    },
    // Page 10 - Sleep Routine 2
    {
      type: 'yesno',
      title: 'Sleep Routine',
      question: 'Do you have a consistent bedtime every night? (within 30 minutes)',
      key: 'consistentBedtime',
      roleMapping: {
        'No': 'Workaholic Wanderer' // Secondary role
      }
    },
    // Page 11 - Caffeine Habits
    {
      type: 'yesno',
      title: 'Caffeine Habits',
      question: 'Do you consume caffeine after 1 PM?',
      key: 'lateCaffeine',
      roleMapping: {
        'Yes': 'Overcaffeinator'
      }
    },
    // Page 13 - Exercising
    {
      type: 'yesno',
      title: 'Exercising',
      question: 'Do you have any physical activity before you go to sleep?',
      key: 'lateExercise',
      roleMapping: {
        'Yes': 'Workaholic Wanderer'
      }
    },
    // Page 14 - Relaxing Activities
    {
      type: 'yesno',
      title: 'Activities',
      question: 'Do you have a relaxing activity you do before you go to sleep?',
      key: 'relaxingActivity',
      roleMapping: {
        'No': 'Digestive Disruptor' // Secondary role
      }
    },
    // Page 15 - Screen Time 1
    {
      type: 'radio',
      title: 'Screen Time Before Bed',
      question: 'On average, how long before bed do you stop using your phone?',
      options: ['> 60 min', '30-60 min', '10-30 min', 'Until I sleep'],
      key: 'screenTime',
      roleMapping: {
        'Until I sleep': 'Screen Zombie',
        '10-30 min': 'Screen Zombie'
      }
    },
    // Page 16 - Screen Time 2
    {
      type: 'yesno',
      title: 'Screen Time Before Bed',
      question: 'Do you use your phone or other screens within an hour of going to sleep?',
      key: 'screenTimeBeforeBed',
      roleMapping: {
        'Yes': 'Screen Zombie'
      }
    },
    // Page 17 - Alarms
    {
      type: 'number',
      title: 'Alarms',
      question: 'How many alarms do you usually need?',
      key: 'alarmCount',
      roleMapping: {
        '>3': 'Sleep-Deprived Dreamer' // Assuming values will be parsed
      }
    },
    // Page 18 - Nap Time
    {
      type: 'yesno',
      title: 'Nap Time',
      question: 'Do you nap during the day?',
      key: 'dayNaps',
      roleMapping: {
        'Yes': 'Fragmented Sleeper' // Secondary role
      }
    },
    // Page 19 - Completion
    {
      type: 'completion',
      title: 'Congratulations!',
      description: 'Get ready to embark on your journey for a better sleep.',
      buttonText: 'CONTINUE TO DASHBOARD'
    }
  ];

  const handleAnswer = (answer) => {
    if (questions[step].key) {
      const newAnswers = { ...answers, [questions[step].key]: answer };
      setAnswers(newAnswers);
      
      // Check for immediate role assignment based on this answer
      const currentQuestion = questions[step];
      if (currentQuestion.roleMapping && currentQuestion.roleMapping[answer]) {
        setUserRole(currentQuestion.roleMapping[answer]);
      }

      // On last question before completion, do final role assignment
      if (step === questions.length - 2) {
        setIsSubmitting(true);
        setApiError(null);
        
        try {
          // Try API first
          const { role, description } = await submitSurveyToAPI(newAnswers);
          setUserRole(role);
        } catch (error) {
          // Fallback to local logic
          setApiError("Connection issue - using local analysis");
          const localRole = assignSleepRole(newAnswers);
          setUserRole(localRole);
        } finally {
          setIsSubmitting(false);
        }
      }
    }

    if (step < questions.length - 1) {
      setStep(step + 1);
      setNumberInput('');
    } else {
      localStorage.setItem('userRole', userRole);
      localStorage.setItem('userSleepData', JSON.stringify(answers));
      navigate(redirectTo, { replace: true });
    }
  };

  const handleNumberInputChange = (e) => {
    setNumberInput(e.target.value);
  };

  const handleNumberSubmit = () => {
    if (numberInput.trim() !== '') {
      handleAnswer(numberInput);
    }
  };

  const currentQuestion = questions[step];

  return (
    <div className="survey-container">
      {currentQuestion.type === 'welcome' ? (
        <div className="question-card welcome">
          <h1>{currentQuestion.title}</h1>
          <p>{currentQuestion.description}</p>
          <button className="primary-button" onClick={() => handleAnswer(null)}>
            {currentQuestion.buttonText}
          </button>
        </div>
      ) : currentQuestion.type === 'completion' ? (
        <div className="question-card completion">
          <h1>{currentQuestion.title}</h1>
          <p>{currentQuestion.description}</p>
          {userRole && (
            <div className="role-result">
              <h3>Your Sleep Type: {userRole}</h3>
              <p>{getRoleDescription(userRole)}</p>
            </div>
          )}
          <button className="primary-button" onClick={() => handleAnswer(null)}>
            {currentQuestion.buttonText}
          </button>
        </div>
      ) : currentQuestion.type === 'radio' ? (
        <div className="question-card">
          <h2>{currentQuestion.title}</h2>
          <p>{currentQuestion.question}</p>
          <div className="radio-options">
            {currentQuestion.options.map(option => (
              <button 
                key={option} 
                onClick={() => handleAnswer(option)}
                className={`radio-button ${answers[currentQuestion.key] === option ? 'selected' : ''}`}
              >
                {option}
              </button>
            ))}
          </div>
        </div>
      ) : currentQuestion.type === 'number' ? (
        <div className="question-card">
          <h2>{currentQuestion.title}</h2>
          <p>{currentQuestion.question}</p>
          <div className="number-input">
            <input 
              type="number" 
              min="0"
              max="20"
              value={numberInput}
              onChange={handleNumberInputChange}
              placeholder="Enter number"
            />
            <button 
              className="continue-button" 
              onClick={handleNumberSubmit}
              disabled={numberInput.trim() === ''}
            >
              Continue
            </button>
          </div>
        </div>
      ) : (
        <div className="question-card">
          <h2>{currentQuestion.title}</h2>
          <p>{currentQuestion.question}</p>
          <div className="yesno-options">
            <button 
              onClick={() => handleAnswer('Yes')}
              className={`yesno-button ${answers[currentQuestion.key] === 'Yes' ? 'selected' : ''}`}
            >
              Yes
            </button>
            <button 
              onClick={() => handleAnswer('No')}
              className={`yesno-button ${answers[currentQuestion.key] === 'No' ? 'selected' : ''}`}
            >
              No
            </button>
          </div>
        </div>
      )}
    </div>
  );
};

export default Survey;