import styled from 'styled-components';
import { theme } from '../../../styles/theme';

const StrengthMeter = styled.div`
  display: flex;
  align-items: center;
  gap: 0.5rem;
  margin-top: 0.5rem;
`;

const Bar = styled.div`
  height: 4px;
  flex: 1;
  background: ${props => props.active ? theme.colors.primary : theme.colors.gray[300]};
  border-radius: 2px;
  transition: background 0.3s;
`;

const StrengthText = styled.span`
  font-size: 0.8rem;
  color: ${theme.colors.primary};
  font-weight: 500;
`;

export default function PasswordStrength({ strength = 0 }) {
  const strengthLabels = ['Weak', 'Fair', 'Good', 'Strong'];
  
  return (
    <StrengthMeter>
      {[...Array(4)].map((_, i) => (
        <Bar key={i} active={strength > i} />
      ))}
      <StrengthText>{strengthLabels[strength] || ''}</StrengthText>
    </StrengthMeter>
  );
}