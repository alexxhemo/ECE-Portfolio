// PRIMARY ROLES (Red - Main issues)
const PRIMARY_ROLES = [
  {
    name: 'Restless Relaxer',
    condition: (answers) => answers.sleepDuration === 'More than 30 min'
  },
  {
    name: 'Fragmented Sleeper',
    condition: (answers) => answers.nightWakeups === 'Yes'
  },
  {
    name: 'Sleep-Deprived Dreamer',
    condition: (answers) => answers.hoursOfSleep === 'Less than 7'
  },
  {
    name: 'Oversleeping Owl', // Only when >9 hours is due to fatigue
    condition: (answers) => answers.hoursOfSleep === 'More than 9' && answers.lateExercise === 'Yes'
  },
  {
    name: 'Overcaffeinator',
    condition: (answers) => answers.lateCaffeine === 'Yes'
  },
  {
    name: 'Workaholic Wanderer',
    condition: (answers) => answers.lateExercise === 'Yes'
  },
  {
    name: 'Screen Zombie',
    condition: (answers) => 
      answers.screenTimeBeforeBed === 'Yes' || 
      answers.screenTime === 'Until I sleep'
  }
];

// SECONDARY ROLES (Yellow - Areas for improvement)
const SECONDARY_ROLES = [
  {
    name: 'Digestive Disruptor',
    condition: (answers) => 
      answers.largeMealsBeforeBed === 'Yes' || 
      answers.alcoholBeforeBed === 'Yes'
  },
  {
    name: 'Workaholic Wanderer', // Secondary trigger
    condition: (answers) => answers.consistentBedtime === 'No'
  }
];

// HEALTHY SLEEPER (Green - Default)
const HEALTHY_SLEEPER = 'Healthy Sleeper';

export const assignSleepRole = (answers) => {
  // 1. Check primary roles first
  for (const role of PRIMARY_ROLES) {
    if (role.condition(answers)) {
      return role.name;
    }
  }

  // 2. Check secondary roles if no primary found
  for (const role of SECONDARY_ROLES) {
    if (role.condition(answers)) {
      return role.name;
    }
  }

  // 3. Default to healthy sleeper
  return HEALTHY_SLEEPER;
};

export const getRoleDescription = (role) => {
  const descriptions = {
    'Restless Relaxer': 'Takes more than 30 minutes to fall asleep (Primary sleep issue)',
    'Fragmented Sleeper': 'Wakes during night with trouble falling back asleep (Primary sleep issue)',
    'Sleep-Deprived Dreamer': 'Gets less than 7 hours of sleep (Primary sleep issue)',
    'Oversleeping Owl': 'Sleeps more than 9 hours due to fatigue (Primary sleep issue)',
    'Overcaffeinator': 'Consumes caffeine within 10 hours of sleep (Primary sleep issue)',
    'Workaholic Wanderer': 'Exercises or works within 2 hours of bedtime (Primary/Secondary issue)',
    'Screen Zombie': 'Uses screens within 1 hour of bedtime (Primary sleep issue)',
    'Digestive Disruptor': 'Consumes large meals or alcohol before bed (Secondary issue)',
    'Healthy Sleeper': 'Maintains generally healthy sleep habits'
  };
  return descriptions[role] || 'No specific sleep issues identified';
};