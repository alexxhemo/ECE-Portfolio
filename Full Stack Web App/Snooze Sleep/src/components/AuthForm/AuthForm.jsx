import styled from 'styled-components';
import { theme } from '../../styles/theme';

const StyledForm = styled.form`
  width: 100%;
  max-width: 420px;
  background: white;
  padding: 2.5rem;
  border-radius: ${theme.radii.lg};
  box-shadow: ${theme.shadows.lg};
  margin: 1rem;

  h1 {
    color: ${theme.colors.primary};
    text-align: center;
    margin-bottom: 2rem;
    font-size: 1.8rem;
  }
`;

const InputGroup = styled.div`
  margin-bottom: 1.5rem;

  label {
    display: block;
    margin-bottom: 0.5rem;
    font-weight: 500;
  }

  input {
    width: 100%;
    padding: 1rem;
    border: 2px solid ${theme.colors.gray[300]};
    border-radius: ${theme.radii.md};
    font-size: 1rem;
    transition: border-color 0.2s;

    &:focus {
      border-color: ${theme.colors.accent};
    }
  }
`;

export { StyledForm as AuthForm, InputGroup };